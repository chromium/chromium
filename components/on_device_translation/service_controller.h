// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_H_
#define COMPONENTS_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_H_

#include <optional>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "components/on_device_translation/installer.h"
#include "components/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/on_device_translation/public/mojom/translator.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace on_device_translation {

class OnDeviceTranslationServiceLauncher;
enum class LanguagePackKey;

class OnDeviceTranslationController {
 public:
  virtual ~OnDeviceTranslationController() = default;

  enum class CreateTranslatorError {
    kInvalidBinary,
    kInvalidFunctionPointer,
    kFailedToInitialize,
    kFailedToCreateTranslator,
    kInvalidVersion,
    kServiceCrashed,
    kNotSupportedLanguage,
    kExceedsServiceCountLimitation,
    kExceedsPendingTaskCountLimitation,
  };

  enum class CanTranslateResult {
    kReadily,
    kAfterDownloadLibraryNotReady,
    kAfterDownloadLibraryAndLanguagePackNotReady,
    kAfterDownloadLanguagePackNotReady,
    kNoNotSupportedLanguage,
    kNoExceedsServiceCountLimitation,
    kNoServiceCrashed,
  };

  using CreateTranslatorCallback = base::OnceCallback<void(
      base::expected<mojo::PendingRemote<mojom::OnDeviceTranslator>,
                     CreateTranslatorError>)>;
  using CanTranslateCallback = base::OnceCallback<void(CanTranslateResult)>;

  virtual bool IsServiceRunning() const = 0;
  // Creates a translator class that implements `mojom::Translator` for the
  // given language pair.
  virtual void CreateTranslator(const std::string& source_lang,
                                const std::string& target_lang,
                                CreateTranslatorCallback callback) = 0;

  // Checks if the translate service can do translation from `source_lang` to
  // `target_lang`.
  virtual void CanTranslate(const std::string& source_lang,
                            const std::string& target_lang,
                            CanTranslateCallback callback) = 0;
};

// This class is the controller that launches the on-device translation service
// and delegates the functionalities. It is designed to be shared by multiple
// `TranslationManagerImpl` instances.  A single instance of this class is
// created for each pair of browser context and origin.
// TODO(crbug.com/364795294): This class does not support Android yet.
class OnDeviceTranslationServiceController
    : public OnDeviceTranslationController,
      public OnDeviceTranslationInstaller::Observer {
 public:
  OnDeviceTranslationServiceController(
      std::unique_ptr<OnDeviceTranslationServiceLauncher> launcher,
      std::string service_display_name_suffix);
  ~OnDeviceTranslationServiceController() override;

  OnDeviceTranslationServiceController(
      const OnDeviceTranslationServiceController&) = delete;
  OnDeviceTranslationServiceController& operator=(
      const OnDeviceTranslationServiceController&) = delete;

  bool IsServiceRunning() const override;
  // Creates a translator class that implements `mojom::Translator` for the
  // given language pair.
  void CreateTranslator(const std::string& source_lang,
                        const std::string& target_lang,
                        CreateTranslatorCallback callback) override;

  // Checks if the translate service can do translation from `source_lang` to
  // `target_lang`.
  void CanTranslate(const std::string& source_lang,
                    const std::string& target_lang,
                    CanTranslateCallback callback) override;

  // Sets the service idle timeout for testing. This must be called before the
  // service is started.
  void SetServiceIdleTimeoutForTesting(base::TimeDelta service_idle_timeout);

  // OnDeviceTranslationInstaller::Observer
  void OnLanguagePackInstalled(const LanguagePackKey lang_pack) override;
  void OnLanguagePackInstallationChanged(
      const LanguagePackKey lang_pack) override;
  void OnInstallationChanged() override;

 private:
  friend base::RefCounted<OnDeviceTranslationServiceController>;

  // The information of a pending task. This is used to keep the tasks that are
  // waiting for the language packs to be installed.
  class PendingTask {
   public:
    PendingTask(std::set<LanguagePackKey> required_packs,
                base::OnceClosure once_closure);
    ~PendingTask();
    PendingTask(const PendingTask&) = delete;
    PendingTask& operator=(const PendingTask&) = delete;

    PendingTask(PendingTask&&);
    PendingTask& operator=(PendingTask&&);

    std::set<LanguagePackKey> required_packs;
    base::OnceClosure once_closure;
  };

  // Checks if the translate service can do translation from `source_lang` to
  // `target_lang`.
  CanTranslateResult CanTranslateImpl(const std::string& source_lang,
                                      const std::string& target_lang);

  // Send the CreateTranslator IPC call to the OnDeviceTranslationService.
  void CreateTranslatorImpl(const std::string& source_lang,
                            const std::string& target_lang,
                            CreateTranslatorCallback callback);

  // Tries to start the service if it is not already running. Returns true if
  // the service is running or is started successfully.
  bool MaybeStartService();

  void MaybeRunPendingTasks();

  // Called when the service is idle and the idle timeout is reached.
  void OnServiceIdle();

  base::RepeatingCallback<bool()> can_start_service_check_;
  base::OnceClosure on_deleted_callback_;

  std::unique_ptr<OnDeviceTranslationServiceLauncher> launcher_;
  // This gets appended to the display name of the service.
  std::string service_display_name_suffix_;
  // The idle timeout for the translation service. When the service is idle for
  // this amount of time, the service will be terminated.
  base::TimeDelta service_idle_timeout_;
  mojo::Remote<mojom::OnDeviceTranslationService> service_remote_;
  // The pending tasks that are waiting for the language packs to be installed.
  std::vector<PendingTask> pending_tasks_;
};

}  // namespace on_device_translation

#endif  // COMPONENTS_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_H_
