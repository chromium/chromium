// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_CLIENT_CONTROLLED_ACCELERATORS_H_
#define COMPONENTS_EXO_CLIENT_CONTROLLED_ACCELERATORS_H_

#include "components/exo/client_controlled_shell_surface.h"
#include "ui/aura/window.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/views/focus/focus_manager.h"

namespace exo {

enum class ClientControlledAcceleratorAction {
  ZOOM_IN,
  ZOOM_OUT,
  ZOOM_RESET,
};

class ClientControlledShellSurface;

class ClientControlledAcceleratorTarget : public ui::AcceleratorTarget {
 public:
  explicit ClientControlledAcceleratorTarget(
      ClientControlledShellSurface* surface);
  ~ClientControlledAcceleratorTarget() override;

  void RegisterAccelerator(const ui::Accelerator& accelerator,
                           ClientControlledAcceleratorAction action);
  void RegisterAccelerator(ui::Accelerator&& accelerator,
                           ClientControlledAcceleratorAction action);

  // ui::AcceleratorTarget interface implementation.
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

 private:
  ClientControlledShellSurface* surface_;
  std::map<ui::Accelerator, ClientControlledAcceleratorAction> accelerators_;

  DISALLOW_COPY_AND_ASSIGN(ClientControlledAcceleratorTarget);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_CLIENT_CONTROLLED_ACCELERATORS_H_
