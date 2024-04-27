// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/app/cast_main_delegate.h"

#include <algorithm>
#include <string>
#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/cpu.h"
#include "base/debug/leak_annotations.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/posix/global_descriptors.h"
#include "build/build_config.h"
#include "chromecast/base/cast_paths.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/process_types.h"
#include "chromecast/browser/cast_content_browser_client.h"
#include "chromecast/browser/cast_feature_list_creator.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/common/cast_resource_delegate.h"
#include "chromecast/common/global_descriptors.h"
#include "chromecast/gpu/cast_content_gpu_client.h"
#include "chromecast/renderer/cast_content_renderer_client.h"
#include "components/crash/core/app/crash_reporter_client.h"
#include "components/crash/core/common/crash_key.h"
#include "content/public/app/initialize_mojo_core.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/apk_assets.h"
#include "chromecast/app/android/cast_crash_reporter_client_android.h"
#include "ui/base/resource/resource_bundle_android.h"
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "chromecast/app/linux/cast_crash_reporter_client.h"
#include "sandbox/policy/switches.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

namespace {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
chromecast::CastCrashReporterClient* GetCastCrashReporter() {
  static base::NoDestructor<chromecast::CastCrashReporterClient>
      crash_reporter_client;
  return crash_reporter_client.get();
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
const int kMaxCrashFiles = 10;
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

namespace chromecast {
namespace shell {

CastMainDelegate::CastMainDelegate() {}

CastMainDelegate::~CastMainDelegate() {}

std::optional<int> CastMainDelegate::BasicStartupComplete() {
  RegisterPathProvider();

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;

  const base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

  // Must be created outside of the if scope below to avoid lifetime concerns.
  std::string log_path_as_string;
  if (command_line->HasSwitch(switches::kLogFile)) {
    auto file_path = command_line->GetSwitchValuePath(switches::kLogFile);
    DCHECK(!file_path.empty());
    log_path_as_string = file_path.value();

    settings.logging_dest = logging::LOG_TO_ALL;
    settings.log_file_path = log_path_as_string.c_str();
    settings.lock_log = logging::DONT_LOCK_LOG_FILE;

    // If this is the browser process, delete the old log file. Else, append to
    // it.
    settings.delete_old = process_type.empty()
                              ? logging::DELETE_OLD_LOG_FILE
                              : logging::APPEND_TO_OLD_LOG_FILE;
  }

#if BUILDFLAG(IS_ANDROID)
  // Browser process logs are recorded for attaching with crash dumps.
  if (process_type.empty()) {
    base::FilePath log_file;
    base::PathService::Get(FILE_CAST_ANDROID_LOG, &log_file);
    settings.logging_dest =
        logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
    log_path_as_string = log_file.value();
    settings.log_file_path = log_path_as_string.c_str();
    settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  }
#endif  // BUILDFLAG(IS_ANDROID)
  logging::InitLogging(settings);
#if BUILDFLAG(IS_CAST_DESKTOP_BUILD)
  logging::SetLogItems(true, true, true, false);
#else
  // Timestamp available through logcat -v time.
  logging::SetLogItems(true, true, false, false);
#endif  // BUILDFLAG(IS_CAST_DESKTOP_BUILD)

#if BUILDFLAG(IS_ANDROID)
  // Only delete the old crash dumps if the current process is the browser
  // process. Empty |process_type| signifies browser process.
  if (process_type.empty()) {
    // Get a listing of all of the crash dump files.
    base::FilePath crash_directory;
    if (CastCrashReporterClientAndroid::GetCrashReportsLocation(
            process_type, &crash_directory)) {
      base::FileEnumerator crash_directory_list(crash_directory, false,
                                                base::FileEnumerator::FILES);
      std::vector<base::FilePath> crash_files;
      for (base::FilePath file = crash_directory_list.Next(); !file.empty();
           file = crash_directory_list.Next()) {
        crash_files.push_back(file);
      }
      // Delete crash dumps except for the |kMaxCrashFiles| most recent ones.
      if (crash_files.size() > kMaxCrashFiles) {
        auto newest_first =
            [](const base::FilePath& l, const base::FilePath& r) -> bool {
              base::File::Info l_info, r_info;
              base::GetFileInfo(l, &l_info);
              base::GetFileInfo(r, &r_info);
              return l_info.last_modified > r_info.last_modified;
            };
        std::partial_sort(crash_files.begin(),
                          crash_files.begin() + kMaxCrashFiles,
                          crash_files.end(), newest_first);
        for (auto file = crash_files.begin() + kMaxCrashFiles;
             file != crash_files.end(); ++file) {
          base::DeleteFile(*file);
        }
      }
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)

  if (settings.logging_dest & logging::LOG_TO_FILE) {
    LOG(INFO) << "Logging to file: " << settings.log_file_path;
  }
  return std::nullopt;
}

void CastMainDelegate::PreSandboxStartup() {
#if defined(ARCH_CPU_ARM_FAMILY) && \
    (BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
  // Create an instance of the CPU class to parse /proc/cpuinfo and cache the
  // results. This data needs to be cached when file-reading is still allowed,
  // since base::CPU expects to be callable later, when file-reading is no
  // longer allowed.
  base::CPU cpu_info;
#endif

  const base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

  bool enable_crash_reporter =
      !command_line->HasSwitch(switches::kDisableCrashReporter);
  if (enable_crash_reporter) {
    // TODO(crbug.com/40188745): Complete crash reporting integration on
    // Fuchsia.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    crash_reporter::SetCrashReporterClient(GetCastCrashReporter());

    if (process_type != switches::kZygoteProcess) {
      CastCrashReporterClient::InitCrashReporter(process_type);
    }
    crash_reporter::InitializeCrashKeys();
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  }

  InitializeResourceBundle();
}

absl::variant<int, content::MainFunctionParams> CastMainDelegate::RunProcess(
    const std::string& process_type,
    content::MainFunctionParams main_function_params) {
#if BUILDFLAG(IS_ANDROID)
  if (!process_type.empty())
    return std::move(main_function_params);

  // Note: Android must handle running its own browser process.
  // See ChromeMainDelegateAndroid::RunProcess.
  browser_runner_ = content::BrowserMainRunner::Create();
  int exit_code = browser_runner_->Initialize(std::move(main_function_params));
  // On Android we do not run BrowserMain(), so the above initialization of a
  // BrowserMainRunner is all we want to occur. Preserve any error codes > 0.
  if (exit_code > 0)
    return exit_code;
  return 0;
#else
  return std::move(main_function_params);
#endif  // BUILDFLAG(IS_ANDROID)
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
void CastMainDelegate::ZygoteForked() {
  const base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());
  bool enable_crash_reporter = !command_line->HasSwitch(
      switches::kDisableCrashReporter);
  if (enable_crash_reporter) {
    std::string process_type =
        command_line->GetSwitchValueASCII(switches::kProcessType);
    CastCrashReporterClient::InitCrashReporter(process_type);
  }
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

bool CastMainDelegate::ShouldCreateFeatureList(InvokedIn invoked_in) {
  return absl::holds_alternative<InvokedInChildProcess>(invoked_in);
}

bool CastMainDelegate::ShouldInitializeMojo(InvokedIn invoked_in) {
  return ShouldCreateFeatureList(invoked_in);
}

std::optional<int> CastMainDelegate::PostEarlyInitialization(
    InvokedIn invoked_in) {
  if (ShouldCreateFeatureList(invoked_in)) {
    // content is handling the feature list.
    return std::nullopt;
  }

  DCHECK(cast_feature_list_creator_);

#if !BUILDFLAG(IS_ANDROID)
  // PrefService requires the home directory to be created before the pref store
  // can be initialized properly.
  base::FilePath home_dir;
  CHECK(base::PathService::Get(DIR_CAST_HOME, &home_dir));
  CHECK(base::CreateDirectory(home_dir));
#endif  // !BUILDFLAG(IS_ANDROID)

  // TODO(crbug.com/40791269): If we're able to create the MetricsStateManager
  // earlier, clean up the below if and else blocks and call
  // MetricsStateManager::InstantiateFieldTrialList().
  //
  // The FieldTrialList is a dependency of the feature list. In tests, it is
  // constructed as part of the test suite.
  const auto* invoked_in_browser =
      absl::get_if<InvokedInBrowserProcess>(&invoked_in);
  if (invoked_in_browser && invoked_in_browser->is_running_test) {
    DCHECK(base::FieldTrialList::GetInstance());
  } else {
    // This is intentionally leaked since it needs to live for the duration of
    // the browser process and there's no benefit to cleaning it up at exit.
    base::FieldTrialList* leaked_field_trial_list = new base::FieldTrialList();
    ANNOTATE_LEAKING_OBJECT_PTR(leaked_field_trial_list);
    std::ignore = leaked_field_trial_list;
  }

  // Initialize the base::FeatureList and the PrefService (which it depends on),
  // so objects initialized after this point can use features from
  // base::FeatureList.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const bool use_browser_config =
      command_line->HasSwitch(switches::kUseCastBrowserPrefConfig);
  ProcessType process_type = use_browser_config ? ProcessType::kCastBrowser
                                                : ProcessType::kCastService;
  cast_feature_list_creator_->CreatePrefServiceAndFeatureList(process_type);

  content::InitializeMojoCore();

  return std::nullopt;
}

void CastMainDelegate::InitializeResourceBundle() {
  base::FilePath pak_file;
  CHECK(base::PathService::Get(FILE_CAST_PAK, &pak_file));
#if BUILDFLAG(IS_ANDROID)
  // On Android, the renderer runs with a different UID and can never access
  // the file system. Use the file descriptor passed in at launch time.
  auto* global_descriptors = base::GlobalDescriptors::GetInstance();
  int pak_fd = global_descriptors->MaybeGet(kAndroidPakDescriptor);
  base::MemoryMappedFile::Region pak_region;
  if (pak_fd >= 0) {
    pak_region = global_descriptors->GetRegion(kAndroidPakDescriptor);

    base::File android_pak_file(pak_fd);
    ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(
        android_pak_file.Duplicate(), pak_region);
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromFileRegion(
        std::move(android_pak_file), pak_region, ui::k100Percent);
    return;
  } else {
    pak_fd = base::android::OpenApkAsset("assets/cast_shell.pak", &pak_region);
    // Loaded from disk for browsertests.
    if (pak_fd < 0) {
      int flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
      pak_fd = base::File(pak_file, flags).TakePlatformFile();
      pak_region = base::MemoryMappedFile::Region::kWholeFile;
    }
    DCHECK_GE(pak_fd, 0);
    global_descriptors->Set(kAndroidPakDescriptor, pak_fd, pak_region);
  }

  ui::SetLocalePaksStoredInApk(true);
#endif  // BUILDFLAG(IS_ANDROID)

  resource_delegate_.reset(new CastResourceDelegate());
  // TODO(gunsch): Use LOAD_COMMON_RESOURCES once ResourceBundle no longer
  // hardcodes resource file names.
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      "en-US", resource_delegate_.get(),
      ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);

#if BUILDFLAG(IS_ANDROID)
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromFileRegion(
      base::File(pak_fd), pak_region, ui::kScaleFactorNone);
#else
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_file, ui::kScaleFactorNone);
#endif  // BUILDFLAG(IS_ANDROID)
}

content::ContentClient* CastMainDelegate::CreateContentClient() {
  return &content_client_;
}

content::ContentBrowserClient* CastMainDelegate::CreateContentBrowserClient() {
  DCHECK(!cast_feature_list_creator_);
  cast_feature_list_creator_ = std::make_unique<CastFeatureListCreator>();
  browser_client_ =
      CastContentBrowserClient::Create(cast_feature_list_creator_.get());
  return browser_client_.get();
}

content::ContentGpuClient* CastMainDelegate::CreateContentGpuClient() {
  gpu_client_ = CastContentGpuClient::Create();
  return gpu_client_.get();
}

content::ContentRendererClient*
CastMainDelegate::CreateContentRendererClient() {
  renderer_client_ = CastContentRendererClient::Create();
  return renderer_client_.get();
}

}  // namespace shell
}  // namespace chromecast
