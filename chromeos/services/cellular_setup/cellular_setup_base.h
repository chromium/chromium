// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CELLULAR_SETUP_CELLULAR_SETUP_BASE_H_
#define CHROMEOS_SERVICES_CELLULAR_SETUP_CELLULAR_SETUP_BASE_H_

#include "base/macros.h"
#include "chromeos/services/cellular_setup/public/mojom/cellular_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {

namespace cellular_setup {

// mojom::CellularSetup implementation which accepts receivers to bind to it.
// This class does not implement any of mojom::CellularSetup's functions, so
// derived classes should override them.
class CellularSetupBase : public mojom::CellularSetup {
 public:
  ~CellularSetupBase() override;

  void BindReceiver(mojo::PendingReceiver<mojom::CellularSetup> receiver);

 protected:
  CellularSetupBase();

 private:
  mojo::ReceiverSet<mojom::CellularSetup> receivers_;

  DISALLOW_COPY_AND_ASSIGN(CellularSetupBase);
};

}  // namespace cellular_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CELLULAR_SETUP_CELLULAR_SETUP_BASE_H_
