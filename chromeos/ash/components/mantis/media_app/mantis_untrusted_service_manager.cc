// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mantis/media_app/mantis_untrusted_service_manager.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/mantis/media_app/mantis_untrusted_service.h"
#include "chromeos/ash/components/mantis/mojom/mantis_service.mojom.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "chromeos/ash/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace ash {
namespace {

using ::ash::media_app_ui::mojom::MantisUntrustedPage;
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

MantisUntrustedServiceManager::MantisUntrustedServiceManager() {
  ash::mojo_service_manager::GetServiceManagerProxy()->Request(
      chromeos::mojo_services::kCrosMantisService, std::nullopt,
      cros_service_.BindNewPipeAndPassReceiver().PassPipe());
  cros_service_.reset_on_disconnect();
}

MantisUntrustedServiceManager::~MantisUntrustedServiceManager() = default;

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
  if (switches::IsMantisSecretKeyMatched()) {
    std::move(callback).Run(true);
    return;
  }

  // TODO(crbug.com/362993438): Check age restriction and region restriction.
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

void MantisUntrustedServiceManager::Create(
    mojo::PendingRemote<MantisUntrustedPage> page,
    CreateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::PendingRemote<mantis::mojom::MantisProcessor> processor;
  // This API is designed by CrOS service to handle multiple calls safely.
  cros_service_->Initialize(
      CreateProgressObserver(std::move(page)),
      processor.InitWithNewPipeAndPassReceiver(),
      base::BindOnce(&MantisUntrustedServiceManager::OnInitializeDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(processor)));
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
      mantis_untrusted_service_->BindNewPipeAndPassRemote()));
}

}  // namespace ash
