// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/service_controller.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/on_device_translation/component_manager.h"
#include "components/on_device_translation/constants.h"
#include "components/on_device_translation/features.h"
#include "components/on_device_translation/file_operation_proxy_impl.h"
#include "components/on_device_translation/installer.h"
#include "components/on_device_translation/metrics.h"
#include "components/on_device_translation/public/language_pack.h"
#include "components/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/on_device_translation/public/mojom/translator.mojom.h"
#include "components/on_device_translation/public/pref_names.h"
#include "components/on_device_translation/service_controller_manager.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_host_passkeys.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom-shared.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif  // BUILDFLAG(IS_WIN)

using blink::mojom::CanCreateTranslatorResult;

namespace on_device_translation {

// The information of a language pack.
struct OnDeviceTranslationServiceController::LanguagePackInfo {
  std::string language1;
  std::string language2;
  base::FilePath package_path;
};

namespace {

using blink::mojom::CreateTranslatorError;
using mojom::CreateTranslatorResult;
using mojom::FileOperationProxy;
using mojom::OnDeviceTranslationLanguagePackage;
using mojom::OnDeviceTranslationLanguagePackagePtr;
using mojom::OnDeviceTranslationServiceConfig;
using mojom::OnDeviceTranslationServiceConfigPtr;

const char kTranslateKitPackagePaths[] = "translate-kit-packages";

// Prefix for the display name of the on-device translation service. The origin
// is appended to the prefix.
const char kOnDeviceTranslationServiceDisplayNamePrefix[] =
    "On-device Translation Service: ";

std::string ToString(base::FilePath path) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/362123222): Get rid of conditional decoding.
  return path.AsUTF8Unsafe();
#else
  return path.value();
#endif  // BUILDFLAG(IS_WIN)
}

std::vector<base::FilePath> GetLanguagePackInfo(
    const std::optional<
        std::vector<OnDeviceTranslationServiceController::LanguagePackInfo>>&
        lang_pack_info,
    std::vector<mojom::OnDeviceTranslationLanguagePackagePtr>& packages) {
  CHECK(packages.empty());
  std::vector<base::FilePath> package_pathes;
  if (lang_pack_info) {
    for (const auto& package : *lang_pack_info) {
      packages.push_back(mojom::OnDeviceTranslationLanguagePackage::New(
          package.language1, package.language2));
      package_pathes.push_back(package.package_path);
    }
    return package_pathes;
  }

  for (const auto& it : kLanguagePackComponentConfigMap) {
    auto file_path =
        OnDeviceTranslationInstaller::GetInstance()->GetLanguagePackPath(
            it.first);
    if (!file_path.empty()) {
      packages.push_back(mojom::OnDeviceTranslationLanguagePackage::New(
          std::string(ToLanguageCode(it.second->language1)),
          std::string(ToLanguageCode(it.second->language2))));
      package_pathes.push_back(file_path);
    }
  }

  return package_pathes;
}

std::optional<
    std::vector<OnDeviceTranslationServiceController::LanguagePackInfo>>
GetLanguagePackInfoFromCommandLine() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kTranslateKitPackagePaths)) {
    return std::nullopt;
  }
  const auto packages_string =
      command_line->GetSwitchValueNative(kTranslateKitPackagePaths);
  std::vector<base::CommandLine::StringType> splitted_strings =
      base::SplitString(packages_string,
#if BUILDFLAG(IS_WIN)
                        L",",
#else   // !BUILDFLAG(IS_WIN)
                        ",",
#endif  // BUILDFLAG(IS_WIN)
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (splitted_strings.size() % 3 != 0) {
    LOG(ERROR) << "Invalid --" << kTranslateKitPackagePaths << " flag.";
    return std::nullopt;
  }

  std::vector<OnDeviceTranslationServiceController::LanguagePackInfo> packages;
  auto it = splitted_strings.begin();
  while (it != splitted_strings.end()) {
    if (!base::IsStringASCII(*it) || !base::IsStringASCII(*(it + 1))) {
      LOG(ERROR) << "Invalid --" << kTranslateKitPackagePaths << " flag.";
      return std::nullopt;
    }
    OnDeviceTranslationServiceController::LanguagePackInfo package;
#if BUILDFLAG(IS_WIN)
    package.language1 = base::WideToUTF8(*(it++));
    package.language2 = base::WideToUTF8(*(it++));
#else  // !BUILDFLAG(IS_WIN)
    package.language1 = *(it++);
    package.language2 = *(it++);
#endif
    package.package_path = base::FilePath(*(it++));
    packages.push_back(std::move(package));
  }
  return packages;
}

LanguagePackRequirements GetLanguagePackRequirements(
    const std::string& source_lang,
    const std::string& target_lang) {
  LanguagePackRequirements language_pack_requirements;

  // Calculate required language packs.
  language_pack_requirements.required_packs =
      CalculateRequiredLanguagePacks(source_lang, target_lang);

  // Calculate required, not installed language packs.
  const auto installed_packs = ComponentManager::GetInstalledLanguagePacks();
  std::ranges::set_difference(
      language_pack_requirements.required_packs, installed_packs,
      std::back_inserter(
          language_pack_requirements.required_not_installed_packs));

  // Calculate to be registered language packs.
  const auto registered_packs = ComponentManager::GetRegisteredLanguagePacks();
  std::ranges::set_difference(
      language_pack_requirements.required_not_installed_packs, registered_packs,
      std::back_inserter(language_pack_requirements.to_be_registered_packs));

  return language_pack_requirements;
}

// Converts on_device_translation::mojom::CreateTranslatorResult to
// blink::mojom::CreateTranslatorError.
CreateTranslatorError ToCreateTranslatorError(CreateTranslatorResult result) {
  switch (result) {
    case CreateTranslatorResult::kSuccess:
      NOTREACHED();
    case CreateTranslatorResult::kErrorInvalidBinary:
      return CreateTranslatorError::kInvalidBinary;
    case CreateTranslatorResult::kErrorInvalidFunctionPointer:
      return CreateTranslatorError::kInvalidFunctionPointer;
    case CreateTranslatorResult::kErrorFailedToInitialize:
      return CreateTranslatorError::kFailedToInitialize;
    case CreateTranslatorResult::kErrorFailedToCreateTranslator:
      return CreateTranslatorError::kFailedToCreateTranslator;
    case CreateTranslatorResult::kErrorInvalidVersion:
      return CreateTranslatorError::kInvalidVersion;
  }
}

}  // namespace

OnDeviceTranslationServiceController::PendingTask::PendingTask(
    std::set<LanguagePackKey> required_packs,
    base::OnceClosure once_closure)
    : required_packs(std::move(required_packs)),
      once_closure(std::move(once_closure)) {}

OnDeviceTranslationServiceController::PendingTask::~PendingTask() = default;
OnDeviceTranslationServiceController::PendingTask::PendingTask(PendingTask&&) =
    default;
OnDeviceTranslationServiceController::PendingTask&
OnDeviceTranslationServiceController::PendingTask::operator=(PendingTask&&) =
    default;

OnDeviceTranslationServiceController::OnDeviceTranslationServiceController(
    PrefService* local_state,
    ServiceControllerManager* manager,
    const url::Origin& origin)
    : manager_(manager),
      origin_(origin),
      service_idle_timeout_(kTranslationAPIServiceIdleTimeout.Get()),
      file_operation_proxy_(nullptr, base::OnTaskRunnerDeleter(nullptr)),
      language_packs_from_command_line_(GetLanguagePackInfoFromCommandLine()) {
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(this);
}

OnDeviceTranslationServiceController::~OnDeviceTranslationServiceController() {
  manager_->OnServiceControllerDeleted(
      origin_, base::PassKey<OnDeviceTranslationServiceController>());
  OnDeviceTranslationInstaller::GetInstance()->RemoveObserver(this);
}

void OnDeviceTranslationServiceController::CreateTranslator(
    const std::string& source_lang,
    const std::string& target_lang,
    base::OnceCallback<
        void(base::expected<mojo::PendingRemote<mojom::Translator>,
                            CreateTranslatorError>)> callback) {
  LanguagePackRequirements language_pack_requirements;

  // If the language packs are set by the command line, we don't need to check
  // the installed language packs.
  if (!language_packs_from_command_line_.has_value()) {
    language_pack_requirements =
        GetLanguagePackRequirements(source_lang, target_lang);
    std::vector<LanguagePackKey> to_be_registered_packs =
        language_pack_requirements.to_be_registered_packs;
    if (!to_be_registered_packs.empty()) {
      for (const auto& language_pack : to_be_registered_packs) {
        RecordLanguagePairUma(
            "Translate.OnDeviceTranslation.Download.LanguagePair",
            GetSourceLanguageCode(language_pack),
            GetTargetLanguageCode(language_pack));
        // Register the language pack component.
        ComponentManager::GetInstance()
            .RegisterTranslateKitLanguagePackComponent(language_pack);
      }
    }
  }

  if (!ComponentManager::HasTranslateKitLibraryPathFromCommandLine()) {
    // Registers the TranslateKit component.
    ComponentManager::GetInstance().RegisterTranslateKitComponent();
  }

  // If there is no TranslateKit or there are required language packs that are
  // not installed, we will wait until they are installed to create the
  // translator.
  if (!OnDeviceTranslationInstaller::GetInstance()->IsInit() ||
      !language_pack_requirements.required_not_installed_packs.empty()) {
    // When the size of pending tasks is too large, we will not queue the new
    // task and handle the request as failure to avoid OOM of the browser
    // process.
    if (pending_tasks_.size() == kMaxPendingTaskCount) {
      std::move(callback).Run(base::unexpected(
          CreateTranslatorError::kExceedsPendingTaskCountLimitation));
      return;
    }
    pending_tasks_.emplace_back(
        language_pack_requirements.required_packs,
        base::BindOnce(
            &OnDeviceTranslationServiceController::CreateTranslatorImpl,
            base::Unretained(this), source_lang, target_lang,
            std::move(callback)));
    return;
  }
  CreateTranslatorImpl(source_lang, target_lang, std::move(callback));
}

void OnDeviceTranslationServiceController::CreateTranslatorImpl(
    const std::string& source_lang,
    const std::string& target_lang,
    base::OnceCallback<
        void(base::expected<mojo::PendingRemote<mojom::Translator>,
                            CreateTranslatorError>)> callback) {
  mojo::PendingRemote<mojom::Translator> pending_remote;
  auto pending_receiver = pending_remote.InitWithNewPipeAndPassReceiver();

  if (!MaybeStartService()) {
    // If the service can't be started, returns `kExceedsServiceCountLimitation`
    // error.
    std::move(callback).Run(base::unexpected(
        CreateTranslatorError::kExceedsServiceCountLimitation));
    return;
  }
  auto callbacks = base::SplitOnceCallback(std::move(callback));
  CHECK(service_remote_);
  service_remote_->CreateTranslator(
      source_lang, target_lang, std::move(pending_receiver),
      mojo::WrapCallbackWithDropHandler(
          base::BindOnce(
              [](base::OnceCallback<void(
                     base::expected<mojo::PendingRemote<mojom::Translator>,
                                    CreateTranslatorError>)> callback,
                 mojo::PendingRemote<mojom::Translator> pending_remote,
                 CreateTranslatorResult result) {
                if (result == CreateTranslatorResult::kSuccess) {
                  std::move(callback).Run(std::move(pending_remote));
                } else {
                  std::move(callback).Run(
                      base::unexpected(ToCreateTranslatorError(result)));
                }
              },
              std::move(callbacks.first), std::move(pending_remote)),
          base::BindOnce(
              std::move(callbacks.second),
              base::unexpected(CreateTranslatorError::kServiceCrashed))));
}

void OnDeviceTranslationServiceController::CanTranslate(
    const std::string& source_lang,
    const std::string& target_lang,
    base::OnceCallback<void(CanCreateTranslatorResult)> callback) {
  if (!language_packs_from_command_line_.has_value()) {
    // If the language packs are not set by the command line, returns the result
    // of CanTranslateImpl().
    std::move(callback).Run(CanTranslateImpl(source_lang, target_lang));
    return;
  }
  // Otherwise, checks the availability of the library and ask the on device
  // translation service.
  if (!OnDeviceTranslationInstaller::GetInstance()->IsInit()) {
    // Note: Strictly saying, returning AfterDownloadLibraryNotReady is not
    // correct. It might happen that the language packs are missing. But it is
    // OK because this only impacts people loading packs from the commandline.
    std::move(callback).Run(
        CanCreateTranslatorResult::kAfterDownloadLibraryNotReady);
    return;
  }

  if (!MaybeStartService()) {
    // If the service can't be started, returns
    // `kNoExceedsServiceCountLimitation`.
    std::move(callback).Run(
        CanCreateTranslatorResult::kNoExceedsServiceCountLimitation);
    return;
  }

  auto callbacks = base::SplitOnceCallback(std::move(callback));
  CHECK(service_remote_);
  service_remote_->CanTranslate(
      source_lang, target_lang,
      mojo::WrapCallbackWithDropHandler(
          base::BindOnce(
              [](base::OnceCallback<void(CanCreateTranslatorResult)> callback,
                 bool result) {
                std::move(callback).Run(
                    result
                        ? CanCreateTranslatorResult::kReadily
                        : CanCreateTranslatorResult::kNoNotSupportedLanguage);
              },
              std::move(callbacks.first)),
          base::BindOnce(std::move(callbacks.second),
                         CanCreateTranslatorResult::kNoServiceCrashed)));
}

CanCreateTranslatorResult
OnDeviceTranslationServiceController::CanTranslateImpl(
    const std::string& source_lang,
    const std::string& target_lang) {
  // Get information on the registration and install status of the language
  // packs required for translation.
  LanguagePackRequirements language_pack_requirements =
      GetLanguagePackRequirements(source_lang, target_lang);

  if (!service_remote_ && !manager_->CanStartNewService()) {
    // If the service can't be started, returns
    // `kNoExceedsServiceCountLimitation`.
    return CanCreateTranslatorResult::kNoExceedsServiceCountLimitation;
  }

  if (language_pack_requirements.required_packs.empty()) {
    // Empty `required_packs` means that the transltion for the specified
    // language pair is not supported.
    return CanCreateTranslatorResult::kNoNotSupportedLanguage;
  }

  if (language_pack_requirements.required_not_installed_packs.empty()) {
    // All required language packages are installed.
    if (!OnDeviceTranslationInstaller::GetInstance()->IsInit()) {
      // The TranslateKit library is not ready.
      return CanCreateTranslatorResult::kAfterDownloadLibraryNotReady;
    }
    // Both the TranslateKit library and the language packs are ready.
    return CanCreateTranslatorResult::kReadily;
  }

  if (!OnDeviceTranslationInstaller::GetInstance()->IsInit()) {
    // Both the TranslateKit library and the language packs are not ready.
    return CanCreateTranslatorResult::
        kAfterDownloadLibraryAndLanguagePackNotReady;
  }
  // The required language packs are not ready.
  return CanCreateTranslatorResult::kAfterDownloadLanguagePackNotReady;
}

void OnDeviceTranslationServiceController::OnLanguagePackInstalled(
    const LanguagePackKey lang_pack) {}

void OnDeviceTranslationServiceController::OnLanguagePackInstallationChanged(
    const LanguagePackKey lang_pack) {
  service_remote_.reset();
  MaybeRunPendingTasks();
}

void OnDeviceTranslationServiceController::OnInstallationChanged() {
  service_remote_.reset();
  MaybeRunPendingTasks();
}

// Called when the TranslateKitBinaryPath pref is changed.
void OnDeviceTranslationServiceController::OnTranslateKitBinaryPathChanged(
    const std::string& pref_name) {
  service_remote_.reset();
  MaybeRunPendingTasks();
}

void OnDeviceTranslationServiceController::MaybeRunPendingTasks() {
  if (pending_tasks_.empty()) {
    return;
  }
  if (!OnDeviceTranslationInstaller::GetInstance()->IsInit()) {
    return;
  }
  const auto installed_packs = ComponentManager::GetInstalledLanguagePacks();
  std::vector<PendingTask> pending_tasks = std::move(pending_tasks_);
  for (auto& task : pending_tasks) {
    if (std::ranges::all_of(task.required_packs.begin(),
                            task.required_packs.end(),
                            [&](const LanguagePackKey& key) {
                              return installed_packs.contains(key);
                            })) {
      std::move(task.once_closure).Run();
    } else {
      pending_tasks_.push_back(std::move(task));
    }
  }
}

bool OnDeviceTranslationServiceController::MaybeStartService() {
  if (service_remote_) {
    return true;
  }

  if (!manager_->CanStartNewService()) {
    return false;
  }

  auto receiver = service_remote_.BindNewPipeAndPassReceiver();
  service_remote_.reset_on_disconnect();
  service_remote_.set_idle_handler(
      service_idle_timeout_,
      base::BindRepeating(&OnDeviceTranslationServiceController::OnServiceIdle,
                          base::Unretained(this)));

  const base::FilePath binary_path =
      OnDeviceTranslationInstaller::GetInstance()->GetLibraryPath();
  CHECK(!binary_path.empty())
      << "Got an empty path to TranslateKit binary on the device.";

  std::vector<std::string> extra_switches;
  extra_switches.push_back(
      base::StrCat({kTranslateKitBinaryPath, "=", ToString(binary_path)}));

  content::ServiceProcessHost::Launch<mojom::OnDeviceTranslationService>(
      std::move(receiver),
      content::ServiceProcessHost::Options()
          .WithDisplayName(
              base::StrCat({kOnDeviceTranslationServiceDisplayNamePrefix,
                            origin_.Serialize()}))
          .WithExtraCommandLineSwitches(extra_switches)
#if BUILDFLAG(IS_WIN)
          .WithPreloadedLibraries(
              {binary_path},
              content::ServiceProcessHostPreloadLibraries::GetPassKey())
#endif
          .Pass());

  auto config = OnDeviceTranslationServiceConfig::New();
  std::vector<base::FilePath> package_pathes =
      GetLanguagePackInfo(language_packs_from_command_line_, config->packages);
  mojo::PendingReceiver<FileOperationProxy> proxy_receiver =
      config->file_operation_proxy.InitWithNewPipeAndPassReceiver();
  service_remote_->SetServiceConfig(std::move(config));

  // Create a task runner to run the FileOperationProxy.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  // Create the FileOperationProxy which lives in the background thread of
  // `task_runner`.
  file_operation_proxy_ =
      std::unique_ptr<FileOperationProxyImpl, base::OnTaskRunnerDeleter>(
          new FileOperationProxyImpl(std::move(proxy_receiver), task_runner,
                                     std::move(package_pathes)),
          base::OnTaskRunnerDeleter(task_runner));
  return true;
}

void OnDeviceTranslationServiceController::OnServiceIdle() {
  service_remote_.reset();
}

void OnDeviceTranslationServiceController::SetServiceIdleTimeoutForTesting(
    base::TimeDelta service_idle_timeout) {
  // To simplify the logic, we only allow the timeout to be set before the
  // service is running.
  CHECK(!IsServiceRunning());
  service_idle_timeout_ = service_idle_timeout;
}

}  // namespace on_device_translation
