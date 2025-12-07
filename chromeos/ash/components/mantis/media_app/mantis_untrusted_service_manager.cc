// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mantis/media_app/mantis_untrusted_service_manager.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/generative_ai_country_restrictions.h"
#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/uuid.h"
#include "chromeos/ash/components/mantis/media_app/mantis_untrusted_service.h"
#include "chromeos/ash/components/mantis/mojom/mantis_service.mojom.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "chromeos/ash/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "chromeos/ash/components/specialized_features/feature_access_checker.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/text_classifier.mojom.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace ash {
namespace {

using ::ash::media_app_ui::mojom::MantisUntrustedPage;
using ::chromeos::machine_learning::mojom::LoadModelResult;
using ::chromeos::machine_learning::mojom::TextClassifier;
using ::mantis::mojom::PlatformModelProgressObserver;

enum class GenAIPhotoEditingSettings {
  kAllowed = 0,                // Allow and improve AI models
  kAllowedWithoutLogging = 1,  // Allow without improving AI models
  kDisabled = 2,               // Do not allow
};

// A helper class that wraps `MantisUntrustedPage::ReportMantisProgress()` as
// `PlatformModelProgressObserver::Progress()`.
class InitializeProgressObserver : public PlatformModelProgressObserver {
 public:
  explicit InitializeProgressObserver(
      mojo::PendingRemote<MantisUntrustedPage> page)
      : page_(std::move(page)) {}

  // implements PlatformModelProgressObserver:
  void Progress(double progress) override {
    page_->ReportMantisProgress(progress);
  }

 private:
  mojo::Remote<MantisUntrustedPage> page_;
};

}  // namespace

MantisUntrustedServiceManager::MantisUntrustedServiceManager(
    std::unique_ptr<specialized_features::FeatureAccessChecker> access_checker)
    : access_checker_(std::move(access_checker)) {
  ash::mojo_service_manager::GetServiceManagerProxy()->Request(
      chromeos::mojo_services::kCrosMantisService, std::nullopt,
      cros_service_.BindNewPipeAndPassReceiver().PassPipe());
  cros_service_.reset_on_disconnect();
  chromeos::machine_learning::ServiceConnection::GetInstance()
      ->BindMachineLearningService(ml_service_.BindNewPipeAndPassReceiver());
}

MantisUntrustedServiceManager::~MantisUntrustedServiceManager() = default;

// static
specialized_features::FeatureAccessConfig
MantisUntrustedServiceManager::GetFeatureAccessConfig() {
  specialized_features::FeatureAccessConfig access_config;
  access_config.capability_callback =
      base::BindRepeating([](AccountCapabilities capabilities) {
        return capabilities.can_use_generative_ai_photo_editing();
      });
  access_config.country_codes = ash::GetGenerativeAiCountryAllowlist();
  return access_config;
}

void MantisUntrustedServiceManager::OnQueryDone(
    base::OnceCallback<void(bool)> callback,
    chromeos::mojo_service_manager::mojom::ErrorOrServiceStatePtr result) {
  if (!result->is_state() || !result->get_state()->is_registered_state()) {
    std::move(callback).Run(false);
    return;
  }
  cros_service_->GetMantisFeatureStatus(base::BindOnce(
      [](base::OnceCallback<void(bool)> callback,
         mantis::mojom::MantisFeatureStatus status) {
        std::move(callback).Run(status ==
                                mantis::mojom::MantisFeatureStatus::kAvailable);
      },
      std::move(callback)));
}

void MantisUntrustedServiceManager::IsAvailable(
    PrefService* pref_service,
    base::OnceCallback<void(bool)> callback) {
  if (!base::FeatureList::IsEnabled(ash::features::kMediaAppImageMantisModel)) {
    std::move(callback).Run(false);
    return;
  }

  specialized_features::FeatureAccessFailureSet failure_set =
      access_checker_->Check();
  if (!failure_set.empty()) {
    std::move(callback).Run(false);
    return;
  }

  if (pref_service->GetInteger(ash::prefs::kGenAIPhotoEditingSettings) ==
      static_cast<int>(GenAIPhotoEditingSettings::kDisabled)) {
    std::move(callback).Run(false);
    return;
  }

  // Query kCrosMantisService first since it might not be available on every
  // devices.
  ash::mojo_service_manager::GetServiceManagerProxy()->Query(
      chromeos::mojo_services::kCrosMantisService,
      base::BindOnce(&MantisUntrustedServiceManager::OnQueryDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

mojo::PendingRemote<PlatformModelProgressObserver>
MantisUntrustedServiceManager::CreateProgressObserver(
    mojo::PendingRemote<media_app_ui::mojom::MantisUntrustedPage> page) {
  mojo::PendingRemote<PlatformModelProgressObserver> progress_observer;
  progress_observers_.Add(
      std::make_unique<InitializeProgressObserver>(std::move(page)),
      progress_observer.InitWithNewPipeAndPassReceiver());
  return progress_observer;
}

mojo::PendingRemote<TextClassifier>
MantisUntrustedServiceManager::GetTextClassifier() {
  mojo::PendingRemote<TextClassifier> text_classifier;
  ml_service_->LoadTextClassifier(
      text_classifier.InitWithNewPipeAndPassReceiver(),
      base::BindOnce([](LoadModelResult result) {
        LOG_IF(ERROR, result != LoadModelResult::OK)
            << "LoadTextClassifier error: " << result;
      }));
  return text_classifier;
}

void MantisUntrustedServiceManager::Create(
    mojo::PendingRemote<MantisUntrustedPage> page,
    const std::optional<base::Uuid>& dlc_uuid,
    CreateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::PendingRemote<mantis::mojom::MantisProcessor> processor;
  // This API is designed by CrOS service to handle multiple calls safely.
  cros_service_->Initialize(
      CreateProgressObserver(std::move(page)),
      processor.InitWithNewPipeAndPassReceiver(), dlc_uuid, GetTextClassifier(),
      base::BindOnce(&MantisUntrustedServiceManager::OnInitializeDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(processor)));
}

void MantisUntrustedServiceManager::ResetService() {
  mantis_untrusted_service_.reset();
}

void MantisUntrustedServiceManager::OnInitializeDone(
    CreateCallback callback,
    mojo::PendingRemote<mantis::mojom::MantisProcessor> processor,
    mantis::mojom::InitializeResult result) {
  if (result != mantis::mojom::InitializeResult::kSuccess) {
    std::move(callback).Run(CreateResult::NewError(result));
    return;
  }
  mantis_untrusted_service_ =
      std::make_unique<MantisUntrustedService>(std::move(processor));
  std::move(callback).Run(CreateResult::NewService(
      mantis_untrusted_service_->BindNewPipeAndPassRemote(
          base::BindOnce(&MantisUntrustedServiceManager::ResetService,
                         weak_ptr_factory_.GetWeakPtr()))));
}

}  // namespace ash
