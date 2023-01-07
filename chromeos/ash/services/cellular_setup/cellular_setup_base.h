// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_CELLULAR_SETUP_BASE_H_
#define CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_CELLULAR_SETUP_BASE_H_

#include "chromeos/ash/services/cellular_setup/public/mojom/cellular_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::cellular_setup {

// mojom::CellularSetup implementation which accepts receivers to bind to it.
// This class does not implement any of mojom::CellularSetup's functions, so
// derived classes should override them.
class CellularSetupBase : public mojom::CellularSetup {
 public:
  CellularSetupBase(const CellularSetupBase&) = delete;
  CellularSetupBase& operator=(const CellularSetupBase&) = delete;

  ~CellularSetupBase() override;

  void BindReceiver(mojo::PendingReceiver<mojom::CellularSetup> receiver);

 protected:
  CellularSetupBase();

 private:
  mojo::ReceiverSet<mojom::CellularSetup> receivers_;
};

}  // namespace ash::cellular_setup

#endif  // CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_CELLULAR_SETUP_BASE_H_
