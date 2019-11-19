// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CELLULAR_SETUP_CELLULAR_SETUP_IMPL_H_
#define CHROMEOS_SERVICES_CELLULAR_SETUP_CELLULAR_SETUP_IMPL_H_

#include <memory>

#include "base/containers/id_map.h"
#include "base/macros.h"
#include "chromeos/services/cellular_setup/cellular_setup_base.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {

namespace cellular_setup {

class OtaActivator;

// Concrete mojom::CellularSetup implementation. This class creates a new
// OtaActivator instance per each StartActivation() invocation and passes a
// pointer back to the client.
class CellularSetupImpl : public CellularSetupBase {
 public:
  CellularSetupImpl();
  ~CellularSetupImpl() override;

 private:
  // mojom::CellularSetup:
  void StartActivation(mojo::PendingRemote<mojom::ActivationDelegate> delegate,
                       StartActivationCallback callback) override;

  void OnActivationAttemptFinished(size_t request_id);

  size_t next_request_id_ = 0u;
  base::IDMap<std::unique_ptr<OtaActivator>, size_t> ota_activator_map_;

  DISALLOW_COPY_AND_ASSIGN(CellularSetupImpl);
};

}  // namespace cellular_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CELLULAR_SETUP_CELLULAR_SETUP_IMPL_H_
