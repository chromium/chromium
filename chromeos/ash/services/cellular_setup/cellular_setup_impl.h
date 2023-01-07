// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_CELLULAR_SETUP_IMPL_H_
#define CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_CELLULAR_SETUP_IMPL_H_

#include <memory>

#include "base/containers/id_map.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/cellular_setup/cellular_setup_base.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash::cellular_setup {

class OtaActivator;

// Concrete mojom::CellularSetup implementation. This class creates a new
// OtaActivator instance per each StartActivation() invocation and passes a
// pointer back to the client.
class CellularSetupImpl : public CellularSetupBase {
 public:
  // Creates an instance with a lifetime that is bound to the connection
  // that is supplying |receiver|.
  static void CreateAndBindToReciever(
      mojo::PendingReceiver<mojom::CellularSetup> receiver);

  CellularSetupImpl(const CellularSetupImpl&) = delete;
  CellularSetupImpl& operator=(const CellularSetupImpl&) = delete;

  ~CellularSetupImpl() override;

 private:
  friend class CellularSetupImplTest;

  // For unit tests.
  CellularSetupImpl();

  // mojom::CellularSetup:
  void StartActivation(mojo::PendingRemote<mojom::ActivationDelegate> delegate,
                       StartActivationCallback callback) override;

  void OnActivationAttemptFinished(size_t request_id);

  size_t next_request_id_ = 0u;
  base::IDMap<std::unique_ptr<OtaActivator>, size_t> ota_activator_map_;
  base::WeakPtrFactory<CellularSetupImpl> weak_ptr_factory_{this};
};

}  // namespace ash::cellular_setup

#endif  // CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_CELLULAR_SETUP_IMPL_H_
