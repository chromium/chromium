// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_CONTEXT_MENU_RUNNER_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_CONTEXT_MENU_RUNNER_H_

#include <memory>

#import "components/remote_cocoa/app_shim/menu_controller_cocoa_delegate_impl.h"
#include "components/remote_cocoa/common/menu.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/cocoa/menu_controller.h"
#include "ui/base/interaction/element_tracker_mac.h"

@class NSWindow;
@class NSView;

namespace remote_cocoa {
class MojoMenuModel;

class ContextMenuRunner : public mojom::Menu {
 public:
  ContextMenuRunner(mojo::PendingRemote<mojom::MenuHost> host,
                    mojo::PendingReceiver<mojom::Menu> receiver);
  ~ContextMenuRunner() override;

  // Shows the menu given by `menu` in `window`, for view `target_view` (where
  // `target_view` is used by AppKit to for example populate the Services
  // submenu). If `target_view` is nil, `window.contentView` is used instead.
  void ShowMenu(mojom::ContextMenuPtr menu,
                NSWindow* window,
                NSView* target_view);

  // mojom::Menu:
  void Cancel() override;
  void UpdateMenuItem(int32_t command_id,
                      bool enabled,
                      bool visible,
                      const std::u16string& label) override;

 private:
  mojo::Receiver<mojom::Menu> receiver_;
  mojo::Remote<mojom::MenuHost> menu_host_;
  std::unique_ptr<MojoMenuModel> menu_model_;
  MenuControllerCocoaDelegateImpl* menu_delegate_ = nil;
  MenuControllerCocoa* menu_controller_ = nil;
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_CONTEXT_MENU_RUNNER_H_
