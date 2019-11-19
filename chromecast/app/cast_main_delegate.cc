// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/app/cast_main_delegate.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/cpu.h"
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
#include "chromecast/browser/cast_content_browser_client.h"
#include "chromecast/browser/cast_feature_list_creator.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/common/cast_resource_delegate.h"
#include "chromecast/common/global_descriptors.h"
#include "chromecast/gpu/cast_content_gpu_client.h"
#include "chromecast/renderer/cast_content_renderer_client.h"
#include "chromecast/utility/cast_content_utility_client.h"
#include "components/crash/content/app/crash_reporter_client.h"
#include "components/crash/core/common/crash_key.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_ANDROID)
#include "base/android/apk_assets.h"
#include "chromecast/app/android/cast_crash_reporter_client_android.h"
#include "chromecast/app/android/crash_handler.h"
#include "ui/base/resource/resource_bundle_android.h"
#elif defined(OS_LINUX)
#include "chromecast/app/linux/cast_crash_reporter_client.h"
#include "services/service_manager/sandbox/switches.h"
#endif  // defined(OS_LINUX)

namespace {

#if defined(OS_LINUX)
chromecast::CastCrashReporterClient* GetCastCrashReporter() {
  static base::NoDestructor<chromecast::CastCrashReporterClient>
      crash_reporter_client;
  return crash_reporter_client.get();
}
#endif  // defined(OS_LINUX)

#if defined(OS_ANDROID)
const int kMaxCrashFiles = 10;
#endif  // defined(OS_ANDROID)

}  // namespace

namespace chromecast {
namespace shell {

CastMainDelegate::CastMainDelegate() {}

CastMainDelegate::~CastMainDelegate() {}

bool CastMainDelegate::BasicStartupComplete(int* exit_code) {
  RegisterPathProvider();

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
#if defined(OS_ANDROID)
  const base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  // Browser process logs are recorded for attaching with crash dumps.
  if (process_type.empty()) {
    base::FilePath log_file;
    base::PathService::Get(FILE_CAST_ANDROID_LOG, &log_file);
    settings.logging_dest =
        logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
    settings.log_file_path = log_file.value().c_str();
    settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  }
#endif  // defined(OS_ANDROID)
  logging::InitLogging(settings);
#if BUILDFLAG(IS_CAST_DESKTOP_BUILD)
  logging::SetLogItems(true, true, true, false);
#else
  // Timestamp available through logcat -v time.
  logging::SetLogItems(true, true, false, false);
#endif  // BUILDFLAG(IS_CAST_DESKTOP_BUILD)

#if defined(OS_ANDROID)
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
          base::DeleteFile(*file, false);
        }
      }
    }
  }
#endif  // defined(OS_ANDROID)

  content::SetContentClient(&content_client_);
  return false;
}

void CastMainDelegate::PreSandboxStartup() {
#if defined(ARCH_CPU_ARM_FAMILY) && (defined(OS_ANDROID) || defined(OS_LINUX))
  // Create an instance of the CPU class to parse /proc/cpuinfo and cache the
  // results. This data needs to be cached when file-reading is still allowed,
  // since base::CPU expects to be callable later, when file-reading is no
  // longer allowed.
  base::CPU cpu_info;
#endif

  const base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

  bool enable_crash_reporter = !command_line->HasSwitch(
      switches::kDisableCrashReporter);
  if (enable_crash_reporter) {
  // TODO(crbug.com/753619): Enable crash reporting on Fuchsia.
#if defined(OS_ANDROID)
    base::FilePath log_file;
    base::PathService::Get(FILE_CAST_ANDROID_LOG, &log_file);
    chromecast::CrashHandler::Initialize(process_type, log_file);
#elif defined(OS_LINUX)
    crash_reporter::SetCrashReporterClient(GetCastCrashReporter());

    if (process_type != service_manager::switches::kZygoteProcess) {
      CastCrashReporterClient::InitCrashReporter(process_type);
    }
#endif  // defined(OS_LINUX)

    crash_reporter::InitializeCrashKeys();
  }

  InitializeResourceBundle();
}

int CastMainDelegate::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
#if defined(OS_ANDROID)
  if (!process_type.empty())
    return -1;

  // Note: Android must handle running its own browser process.
  // See ChromeMainDelegateAndroid::RunProcess.
  browser_runner_ = content::BrowserMainRunner::Create();
  int exit_code = browser_runner_->Initialize(main_function_params);
  // On Android we do not run BrowserMain(), so the above initialization of a
  // BrowserMainRunner is all we want to occur. Return >= 0 to avoid running
  // BrowserMain, while preserving any error codes > 0.
  if (exit_code > 0)
    return exit_code;
  return 0;
#else
  return -1;
#endif  // defined(OS_ANDROID)
}

#if defined(OS_LINUX)
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
#endif  // defined(OS_LINUX)

bool CastMainDelegate::ShouldCreateFeatureList() {
  return false;
}

void CastMainDelegate::PostEarlyInitialization(bool is_running_tests) {
  DCHECK(cast_feature_list_creator_);

#if !defined(OS_ANDROID)
  // PrefService requires home directory to be created before the pref
  // store can be initialized properly.
  base::FilePath home_dir;
  CHECK(base::PathService::Get(DIR_CAST_HOME, &home_dir));
  CHECK(base::CreateDirectory(home_dir));
#endif  // !defined(OS_ANDROID)

  // The |FieldTrialList| is a dependency of the feature list. In tests, it
  // gets constructed as part of the test suite.
  if (is_running_tests) {
    DCHECK(base::FieldTrialList::GetInstance());
  } else {
    field_trial_list_ = std::make_unique<base::FieldTrialList>(nullptr);
  }

  // Initialize the base::FeatureList and the PrefService (which it depends on),
  // so objects initialized after this point can use features from
  // base::FeatureList.
  cast_feature_list_creator_->CreatePrefServiceAndFeatureList();
}

void CastMainDelegate::InitializeResourceBundle() {
  base::FilePath pak_file;
  CHECK(base::PathService::Get(FILE_CAST_PAK, &pak_file));
#if defined(OS_ANDROID)
  // On Android, the renderer runs with a different UID and can never access
  // the file system. Use the file descriptor passed in at launch time.
  auto* global_descriptors = base::GlobalDescriptors::GetInstance();
  int pak_fd = global_descriptors->MaybeGet(kAndroidPakDescriptor);
  base::MemoryMappedFile::Region pak_region;
  if (pak_fd >= 0) {
    pak_region = global_descriptors->GetRegion(kAndroidPakDescriptor);
    ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(base::File(pak_fd),
                                                            pak_region);
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromFileRegion(
        base::File(pak_fd), pak_region, ui::SCALE_FACTOR_100P);
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
#endif  // defined(OS_ANDROID)

  resource_delegate_.reset(new CastResourceDelegate());
  // TODO(gunsch): Use LOAD_COMMON_RESOURCES once ResourceBundle no longer
  // hardcodes resource file names.
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      "en-US", resource_delegate_.get(),
      ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);

#if defined(OS_ANDROID)
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromFileRegion(
      base::File(pak_fd), pak_region, ui::SCALE_FACTOR_NONE);
#else
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_file, ui::SCALE_FACTOR_NONE);
#endif  // defined(OS_ANDROID)
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

content::ContentUtilityClient* CastMainDelegate::CreateContentUtilityClient() {
  utility_client_ = CastContentUtilityClient::Create();
  return utility_client_.get();
}

}  // namespace shell
}  // namespace chromecast
