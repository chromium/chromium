// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MANTIS_MEDIA_APP_MANTIS_UNTRUSTED_SERVICE_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_MANTIS_MEDIA_APP_MANTIS_UNTRUSTED_SERVICE_MANAGER_H_

#include <memory>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/mantis/media_app/mantis_untrusted_service.h"
#include "chromeos/ash/components/mantis/mojom/mantis_service.mojom.h"
#include "chromeos/ash/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// A component that manages the creation and lifetime of a
// `MantisUntrustedService`.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_MANTIS_MEDIA_APP)
    MantisUntrustedServiceManager {
 public:
  using CreateResult = media_app_ui::mojom::MantisUntrustedServiceResult;
  using CreateCallback =
      base::OnceCallback<void(mojo::StructPtr<CreateResult>)>;

  MantisUntrustedServiceManager();
  MantisUntrustedServiceManager(const MantisUntrustedServiceManager&) = delete;
  MantisUntrustedServiceManager& operator=(
      const MantisUntrustedServiceManager&) = delete;
  ~MantisUntrustedServiceManager();

  void IsAvailable(base::OnceCallback<void(bool)> callback);
  void Create(CreateCallback callback);

 private:
  void OnQueryDone(
      base::OnceCallback<void(bool)> callback,
      chromeos::mojo_service_manager::mojom::ErrorOrServiceStatePtr result);
  void OnInitializeDone(
      CreateCallback callback,
      mojo::PendingRemote<mantis::mojom::MantisProcessor> processor,
      mantis::mojom::InitializeResult result);

  mojo::Remote<mantis::mojom::MantisService> cros_service_;
  std::unique_ptr<MantisUntrustedService> mantis_untrusted_service_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<MantisUntrustedServiceManager> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MANTIS_MEDIA_APP_MANTIS_UNTRUSTED_SERVICE_MANAGER_H_
