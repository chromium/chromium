// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/service_controller.h"

#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/on_device_translation/constants.h"
#include "components/on_device_translation/features.h"
#include "components/on_device_translation/installer.h"
#include "components/on_device_translation/metrics.h"
#include "components/on_device_translation/public/language_pack.h"
#include "components/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/on_device_translation/public/mojom/translator.mojom.h"
#include "components/on_device_translation/public/pref_names.h"
#include "components/on_device_translation/service/service_launcher.h"
#include "components/on_device_translation/translation_manager_util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif  // BUILDFLAG(IS_WIN)

namespace on_device_translation {
namespace {

using mojom::CreateTranslatorResult;
using mojom::FileOperationProxy;
using mojom::OnDeviceTranslationLanguagePackage;
using mojom::OnDeviceTranslationLanguagePackagePtr;
using mojom::OnDeviceTranslationServiceConfig;
using mojom::OnDeviceTranslationServiceConfigPtr;

constexpr char kMemoryConsumerName[] = "OnDeviceTranslationServiceController";

constexpr base::MemoryConsumerTraits kMemoryConsumerTraits(
    // A service process holds about 79 to 84 MiB per loaded language pair.
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kLarge,
    // Ending an idle service process frees all of its memory.
    base::MemoryConsumerTraits::ReleaseMemoryDataAccess::kNone,
    // An idle service has no translator, so nothing is lost.
    base::MemoryConsumerTraits::InformationRetention::kLossless,
    base::MemoryConsumerTraits::ExecutionType::kSynchronous,
    base::MemoryConsumerTraits::InProcess::kNo,
    // `OnServiceIdle()` and `OnReleaseMemory()` check `memory_limit()` to stop
    // keeping idle services warm whenever the limit is below default.
    base::MemoryConsumerTraits::SupportsMemoryLimit::kYes,
    // The next translator needs a new process.
    base::MemoryConsumerTraits::RecreateMemoryCost::kExpensive,
    // Keeps an idle service warm only while `memory_limit()` is at default.
    base::MemoryConsumerTraits::IsStateful::kYes);

constexpr std::string_view kCreateTranslatorMetricActionName =
    "CreateTranslator";
constexpr std::string_view kCreateFailedLanguagePairMetricName =
    "Translate.OnDeviceTranslation.CreateFailed.LanguagePair";
// Time from sending `CreateTranslator()` to the service, including starting
// the service if needed, to its successful reply.
constexpr std::string_view kCreateTranslatorEngineLatencyMetricName =
    "Translate.OnDeviceTranslation.CreateTranslator.EngineLatency";

constexpr std::string_view
    kCreateTranslatorSourceLanguageIsSupportedMetricName =
        "Translate.OnDeviceTranslation.CreateTranslator."
        "IsSourceLanguageSupported";
constexpr std::string_view
    kCreateTranslatorTargetLanguageIsSupportedMetricName =
        "Translate.OnDeviceTranslation.CreateTranslator."
        "IsTargetLanguageSupported";

// Converts on_device_translation::mojom::CreateTranslatorResult to
// OnDeviceTranslationController::CreateTranslatorError.
OnDeviceTranslationController::CreateTranslatorError ToCreateTranslatorError(
    CreateTranslatorResult result) {
  switch (result) {
    case CreateTranslatorResult::kSuccess:
      NOTREACHED();
    case CreateTranslatorResult::kErrorInvalidBinary:
      return OnDeviceTranslationController::CreateTranslatorError::
          kInvalidBinary;
    case CreateTranslatorResult::kErrorInvalidFunctionPointer:
      return OnDeviceTranslationController::CreateTranslatorError::
          kInvalidFunctionPointer;
    case CreateTranslatorResult::kErrorFailedToInitialize:
      return OnDeviceTranslationController::CreateTranslatorError::
          kFailedToInitialize;
    case CreateTranslatorResult::kErrorFailedToCreateTranslator:
      return OnDeviceTranslationController::CreateTranslatorError::
          kFailedToCreateTranslator;
    case CreateTranslatorResult::kErrorInvalidVersion:
      return OnDeviceTranslationController::CreateTranslatorError::
          kInvalidVersion;
  }
}

// Converts an install error to the error of the `create()` calls that wait for
// the install. `failed_error` is the error for `InstallError::kFailed`.
OnDeviceTranslationController::CreateTranslatorError ToCreateTranslatorError(
    InstallError error,
    OnDeviceTranslationController::CreateTranslatorError failed_error) {
  switch (error) {
    case InstallError::kFailed:
      return failed_error;
    case InstallError::kNotEnoughDiskSpace:
      return OnDeviceTranslationController::CreateTranslatorError::
          kNotEnoughDiskSpace;
    case InstallError::kOffline:
      return OnDeviceTranslationController::CreateTranslatorError::kOffline;
    case InstallError::kDataSaverEnabled:
      return OnDeviceTranslationController::CreateTranslatorError::
          kDataSaverEnabled;
    case InstallError::kMeteredConnectionBlocked:
      return OnDeviceTranslationController::CreateTranslatorError::
          kMeteredConnectionBlocked;
  }
}

}  // namespace

OnDeviceTranslationServiceController::PendingTask::PendingTask(
    std::set<LanguagePackKey> required_packs,
    base::OnceClosure once_closure,
    base::OnceCallback<void(CreateTranslatorError)> on_failure)
    : required_packs(std::move(required_packs)),
      once_closure(std::move(once_closure)),
      on_failure(std::move(on_failure)) {}

OnDeviceTranslationServiceController::PendingTask::~PendingTask() = default;
OnDeviceTranslationServiceController::PendingTask::PendingTask(PendingTask&&) =
    default;
OnDeviceTranslationServiceController::PendingTask&
OnDeviceTranslationServiceController::PendingTask::operator=(PendingTask&&) =
    default;

OnDeviceTranslationServiceController::OnDeviceTranslationServiceController(
    std::unique_ptr<OnDeviceTranslationServiceLauncher> launcher,
    std::string service_display_name_suffix,
    OnDeviceTranslationInstaller* installer)
    : launcher_(std::move(launcher)),
      service_display_name_suffix_(service_display_name_suffix),
      service_idle_timeout_(kTranslationAPIServiceIdleTimeout.Get()),
      installer_(installer) {
  CHECK(installer_);
  installer_->AddObserver(this);
  UpdateMemoryConsumerRegistration();
}

OnDeviceTranslationServiceController::~OnDeviceTranslationServiceController() {
  installer_->RemoveObserver(this);

  // Settle the requests that still wait, so that their promises don't hang.
  RejectPendingTasks([](const PendingTask&) { return true; },
                     CreateTranslatorError::kExceedsServiceCountLimitation);
}

void OnDeviceTranslationServiceController::CreateTranslator(
    const std::string& source_lang,
    const std::string& target_lang,
    base::OnceCallback<
        void(base::expected<mojo::PendingRemote<mojom::OnDeviceTranslator>,
                            CreateTranslatorError>)> callback) {
  std::optional<std::string> best_fit_source_language =
      GetBestFitLanguageCode(source_lang);
  std::optional<std::string> best_fit_target_language =
      GetBestFitLanguageCode(target_lang);
  base::UmaHistogramBoolean(
      kCreateTranslatorSourceLanguageIsSupportedMetricName,
      best_fit_source_language.has_value());
  base::UmaHistogramBoolean(
      kCreateTranslatorTargetLanguageIsSupportedMetricName,
      best_fit_target_language.has_value());
  if (!best_fit_source_language.has_value() ||
      !best_fit_target_language.has_value()) {
    std::move(callback).Run(
        base::unexpected(CreateTranslatorError::kNotSupportedLanguage));
    return;
  }

  RecordOnDeviceTranslationCallForLanguagePair(
      kCreateTranslatorMetricActionName, *best_fit_source_language,
      *best_fit_target_language);

  if (!installer_->IsSupported()) {
    // Don't download language packs that can't be used.
    std::move(callback).Run(
        base::unexpected(CreateTranslatorError::kUnsupportedDevice));
    return;
  }

  LanguagePackRequirements language_pack_requirements =
      GetLanguagePackRequirements(*installer_, *best_fit_source_language,
                                  *best_fit_target_language);
  if (!language_pack_requirements.to_be_registered_packs.empty()) {
    const int max_packs = kTranslationAPIMaxInstalledLanguagePacks.Get();
    if (max_packs > 0) {
      std::set<LanguagePackKey> existing_packs =
          installer_->InstalledLanguagePacks();
      existing_packs.merge(installer_->RegisteredLanguagePacks());
      if (existing_packs.size() +
              language_pack_requirements.to_be_registered_packs.size() >
          static_cast<size_t>(max_packs)) {
        RecordLanguagePairUma(
            "Translate.OnDeviceTranslation.DownloadExceedLimit.LanguagePair",
            *best_fit_source_language, *best_fit_target_language);
        std::move(callback).Run(base::unexpected(
            CreateTranslatorError::kExceedsLanguagePackCountLimitation));
        return;
      }
    }
  }
  // Ask for every missing language pack, including the ones that were
  // registered before, so that a download that failed earlier is retried and
  // reports its outcome to this request.
  for (const auto& language_pack :
       language_pack_requirements.required_not_installed_packs) {
    installer_->InstallLanguagePack(language_pack);
  }

  // Registers the TranslateKit component.
  installer_->Init(base::RepeatingClosure());

  // If there is no TranslateKit or there are required language packs that are
  // not installed, we will wait until they are installed to create the
  // translator.
  if (!installer_->IsInit() ||
      !language_pack_requirements.required_not_installed_packs.empty()) {
    // When the size of pending tasks is too large, we will not queue the new
    // task and handle the request as failure to avoid OOM of the browser
    // process.
    if (pending_tasks_.size() == kMaxPendingTaskCount) {
      std::move(callback).Run(base::unexpected(
          CreateTranslatorError::kExceedsPendingTaskCountLimitation));
      return;
    }
    auto callbacks = base::SplitOnceCallback(std::move(callback));
    pending_tasks_.emplace_back(
        language_pack_requirements.required_packs,
        base::BindOnce(
            &OnDeviceTranslationServiceController::CreateTranslatorImpl,
            base::Unretained(this), *best_fit_source_language,
            *best_fit_target_language, std::move(callbacks.first)),
        base::BindOnce(
            [](CreateTranslatorCallback callback, CreateTranslatorError error) {
              std::move(callback).Run(base::unexpected(error));
            },
            std::move(callbacks.second)));
    return;
  }
  CreateTranslatorImpl(*best_fit_source_language, *best_fit_target_language,
                       std::move(callback));
}

void OnDeviceTranslationServiceController::CreateTranslatorImpl(
    const std::string& source_lang,
    const std::string& target_lang,
    base::OnceCallback<
        void(base::expected<mojo::PendingRemote<mojom::OnDeviceTranslator>,
                            CreateTranslatorError>)> callback) {
  mojo::PendingRemote<mojom::OnDeviceTranslator> pending_remote;
  auto pending_receiver = pending_remote.InitWithNewPipeAndPassReceiver();

  // Whether this request reuses a running service, or pays for a process
  // launch, the library load and the pack config.
  const bool service_was_running = IsServiceRunning();
  const base::TimeTicks start_time = base::TimeTicks::Now();
  if (!MaybeStartService()) {
    // If the service can't be started, returns
    // `kExceedsServiceCountLimitation` error.
    std::move(callback).Run(base::unexpected(
        CreateTranslatorError::kExceedsServiceCountLimitation));
    return;
  }
  auto callbacks = base::SplitOnceCallback(std::move(callback));
  CHECK(service_remote_);
  service_idle_timer_.Stop();
  service_remote_->CreateTranslator(
      source_lang, target_lang, std::move(pending_receiver),
      mojo::WrapCallbackWithDropHandler(
          base::BindOnce(
              [](const std::string& source_lang, const std::string& target_lang,
                 bool service_was_running, base::TimeTicks start_time,
                 base::OnceCallback<void(
                     base::expected<
                         mojo::PendingRemote<mojom::OnDeviceTranslator>,
                         CreateTranslatorError>)> callback,
                 mojo::PendingRemote<mojom::OnDeviceTranslator> pending_remote,
                 CreateTranslatorResult result) {
                if (result == CreateTranslatorResult::kSuccess) {
                  base::UmaHistogramMediumTimes(
                      base::StrCat({kCreateTranslatorEngineLatencyMetricName,
                                    service_was_running ? ".ServiceRunning"
                                                        : ".ServiceStarted"}),
                      base::TimeTicks::Now() - start_time);
                  std::move(callback).Run(std::move(pending_remote));
                } else {
                  RecordLanguagePairUma(kCreateFailedLanguagePairMetricName,
                                        source_lang, target_lang);
                  std::move(callback).Run(
                      base::unexpected(ToCreateTranslatorError(result)));
                }
              },
              source_lang, target_lang, service_was_running, start_time,
              std::move(callbacks.first), std::move(pending_remote)),
          base::BindOnce(
              std::move(callbacks.second),
              base::unexpected(CreateTranslatorError::kServiceCrashed))));
}

void OnDeviceTranslationServiceController::CanTranslate(
    const std::string& source_lang,
    const std::string& target_lang,
    CanTranslateCallback callback) {
  CanTranslateWithInstaller(*installer_, source_lang, target_lang,
                            std::move(callback));
}

void OnDeviceTranslationServiceController::OnLanguagePackInstalled(
    const LanguagePackKey lang_pack) {}

void OnDeviceTranslationServiceController::OnLanguagePackInstallationChanged(
    const LanguagePackKey lang_pack) {
  UpdateRunningServiceLanguagePack(lang_pack);
  MaybeRunPendingTasks();
}

void OnDeviceTranslationServiceController::OnInstallationChanged() {
  ResetService();
  MaybeRunPendingTasks();
}

void OnDeviceTranslationServiceController::OnLibraryInstallFailed(
    InstallError error) {
  if (installer_->IsInit()) {
    return;
  }
  // Every pending task waits for the library, so none of them can complete.
  // If the device has no usable library, report `kUnsupportedDevice`.
  const CreateTranslatorError translator_error =
      !installer_->IsSupported()
          ? CreateTranslatorError::kUnsupportedDevice
          : ToCreateTranslatorError(
                error, CreateTranslatorError::kLibraryInstallFailed);
  RejectPendingTasks([](const PendingTask&) { return true; }, translator_error);
}

void OnDeviceTranslationServiceController::OnLanguagePackInstallFailed(
    LanguagePackKey language_pack,
    InstallError error) {
  if (installer_->InstalledLanguagePacks().contains(language_pack)) {
    return;
  }
  RejectPendingTasks(
      [language_pack](const PendingTask& task) {
        return task.required_packs.contains(language_pack);
      },
      ToCreateTranslatorError(
          error, CreateTranslatorError::kLanguagePackDownloadFailed));
}

void OnDeviceTranslationServiceController::RejectPendingTasks(
    base::FunctionRef<bool(const PendingTask&)> predicate,
    CreateTranslatorError error) {
  std::vector<PendingTask> rejected_tasks;
  std::vector<PendingTask> pending_tasks = std::exchange(pending_tasks_, {});
  for (auto& task : pending_tasks) {
    if (predicate(task)) {
      rejected_tasks.push_back(std::move(task));
    } else {
      pending_tasks_.push_back(std::move(task));
    }
  }
  // Run the callbacks last, because they may call back into this class.
  for (auto& task : rejected_tasks) {
    std::move(task.on_failure).Run(error);
  }
}

void OnDeviceTranslationServiceController::UpdateRunningServiceLanguagePack(
    LanguagePackKey lang_pack) {
  if (!service_remote_) {
    return;
  }
  const bool installed =
      installer_->InstalledLanguagePacks().contains(lang_pack);
  const bool service_has_pack = std::ranges::contains(
      service_language_packs_, lang_pack, &LanguagePackPath::first);
  if (installed && !service_has_pack) {
    base::FilePath path = installer_->IsInit()
                              ? installer_->GetLanguagePackPath(lang_pack)
                              : base::FilePath();
    if (path.empty()) {
      // The path of the new language pack isn't known yet. The next request
      // starts a new service with every installed pack.
      ResetService();
      return;
    }

    // Add the new language pack to the running service. Restarting the
    // service would break the translators of every page that uses it. The
    // packs that the service has keep the paths that it got, even if a
    // newer version was installed since.
    service_language_packs_.emplace_back(lang_pack, std::move(path));
    launcher_->UpdateLanguagePacks(*service_remote_.get(),
                                   service_language_packs_);
    return;
  }
  if (!installed && service_has_pack) {
    // The files of the language pack are gone, so translators that use it
    // can't work. The next request starts a new service.
    ResetService();
  }

  // Otherwise, when a language pack that the service has is updated, the
  // component updater keeps the old version until the next browser start, so
  // the service keeps using it. The next service gets the new version.
}

void OnDeviceTranslationServiceController::MaybeRunPendingTasks() {
  if (pending_tasks_.empty()) {
    return;
  }
  if (!installer_->IsInit()) {
    return;
  }
  const auto installed_packs = installer_->InstalledLanguagePacks();
  std::vector<PendingTask> pending_tasks = std::exchange(pending_tasks_, {});
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
// Returns true if the service is running.
bool OnDeviceTranslationServiceController::IsServiceRunning() const {
  return !!service_remote_;
}

bool OnDeviceTranslationServiceController::IsIdle() const {
  // A service with no live translator only holds memory. It can be running,
  // for example after `CreateTranslator()` failed in it.
  return pending_tasks_.empty() &&
         (!service_remote_ || service_idle_timer_.IsRunning());
}

bool OnDeviceTranslationServiceController::MaybeStartService() {
  if (service_remote_) {
    return true;
  }

  std::vector<LanguagePackPath> language_packs;
  for (LanguagePackKey language_pack : installer_->InstalledLanguagePacks()) {
    base::FilePath path = installer_->GetLanguagePackPath(language_pack);
    if (!path.empty()) {
      language_packs.emplace_back(language_pack, std::move(path));
    }
  }
  mojo::PendingRemote<mojom::OnDeviceTranslationService> pending_remote =
      launcher_->Launch(service_display_name_suffix_, installer_,
                        language_packs);
  if (!pending_remote.is_valid()) {
    return false;
  }
  service_language_packs_ = std::move(language_packs);

  service_remote_.Bind(std::move(pending_remote));
  service_remote_.set_disconnect_handler(
      base::BindOnce(&OnDeviceTranslationServiceController::ResetService,
                     base::Unretained(this)));
  service_idle_timer_.Stop();
  // Use a zero Mojo idle timeout so the browser receives the idle edge
  // immediately for memory-pressure eviction and owns the idle countdown.
  service_remote_.set_idle_handler(
      base::TimeDelta(),
      base::BindRepeating(&OnDeviceTranslationServiceController::OnServiceIdle,
                          base::Unretained(this)));

  return true;
}

void OnDeviceTranslationServiceController::UpdateMemoryConsumerRegistration() {
  if (!service_idle_timeout_.is_positive()) {
    memory_consumer_registration_.reset();
    return;
  }
  if (!memory_consumer_registration_) {
    memory_consumer_registration_.emplace(kMemoryConsumerName,
                                          kMemoryConsumerTraits, this);
  }
}

void OnDeviceTranslationServiceController::OnServiceIdle() {
  if (!service_idle_timeout_.is_positive() ||
      memory_limit() < base::MemoryLimit::Default()) {
    ResetService();
    return;
  }
  if (service_idle_timer_.IsRunning()) {
    return;
  }
  service_idle_timer_.Start(
      FROM_HERE, service_idle_timeout_,
      base::BindOnce(&OnDeviceTranslationServiceController::ResetService,
                     base::Unretained(this)));
}

void OnDeviceTranslationServiceController::ResetService() {
  service_remote_.reset();
  service_idle_timer_.Stop();
}

void OnDeviceTranslationServiceController::OnReleaseMemory() {
  if (service_idle_timer_.IsRunning() &&
      memory_limit() < base::MemoryLimit::Default()) {
    ResetService();
  }
}

void OnDeviceTranslationServiceController::OnUpdateMemoryLimit() {}

void OnDeviceTranslationServiceController::SetServiceIdleTimeoutForTesting(
    base::TimeDelta service_idle_timeout) {
  // To simplify the logic, we only allow the timeout to be set before the
  // service is running.
  CHECK(!IsServiceRunning());
  service_idle_timeout_ = service_idle_timeout;
  UpdateMemoryConsumerRegistration();
}

}  // namespace on_device_translation
