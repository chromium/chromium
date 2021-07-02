// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DEMO_MODE_APP_UI_DEMO_MODE_PAGE_HANDLER_H_
#define CHROMEOS_COMPONENTS_DEMO_MODE_APP_UI_DEMO_MODE_PAGE_HANDLER_H_

#include "chromeos/components/demo_mode_app_ui/mojom/demo_mode_app_ui.mojom.h"

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

class DemoModePageHandler : public mojom::demo_mode::PageHandler {
 public:
  DemoModePageHandler(
      mojo::PendingReceiver<mojom::demo_mode::PageHandler> pending_receiver,
      views::Widget* widget);
  ~DemoModePageHandler() override;

  DemoModePageHandler(const PageHandler&) = delete;
  DemoModePageHandler& operator=(const PageHandler&) = delete;

 private:
  // Switch between fullscreen and not-fullscreen
  void ToggleFullscreen() override;

  mojo::Receiver<mojom::demo_mode::PageHandler> receiver_;

  views::Widget* widget_;
};
}  // namespace chromeos
#endif  // CHROMEOS_COMPONENTS_DEMO_MODE_APP_UI_DEMO_MODE_PAGE_HANDLER_H_
