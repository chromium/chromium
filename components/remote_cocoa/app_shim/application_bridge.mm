// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/application_bridge.h"

#include <tuple>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "components/remote_cocoa/app_shim/alert.h"
#include "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "components/remote_cocoa/app_shim/native_widget_ns_window_host_helper.h"
#include "components/system_media_controls/mac/remote_cocoa/system_media_controls_bridge.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/cocoa/remote_accessibility_api.h"

namespace remote_cocoa {

namespace {

class NativeWidgetBridgeOwner : public NativeWidgetNSWindowHostHelper {
 public:
  NativeWidgetBridgeOwner(
      uint64_t bridge_id,
      mojo::PendingAssociatedReceiver<mojom::NativeWidgetNSWindow>
          bridge_receiver,
      mojo::PendingAssociatedRemote<mojom::NativeWidgetNSWindowHost>
          host_remote,
      mojo::PendingAssociatedRemote<mojom::TextInputHost>
          text_input_host_remote) {
    host_remote_.Bind(std::move(host_remote),
                      ui::WindowResizeHelperMac::Get()->task_runner());
    text_input_host_remote_.Bind(
        std::move(text_input_host_remote),
        ui::WindowResizeHelperMac::Get()->task_runner());
    bridge_ = std::make_unique<NativeWidgetNSWindowBridge>(
        bridge_id, host_remote_.get(), this, text_input_host_remote_.get());
    bridge_->BindReceiver(
        std::move(bridge_receiver),
        base::BindOnce(&NativeWidgetBridgeOwner::OnMojoDisconnect,
                       base::Unretained(this)));
  }

 private:
  ~NativeWidgetBridgeOwner() override = default;

  void OnMojoDisconnect() { delete this; }

  // NativeWidgetNSWindowHostHelper:
  id GetNativeViewAccessible() override {
    if (!remote_accessibility_element_) {
      base::ProcessId browser_pid = base::kNullProcessId;
      std::vector<uint8_t> element_token;
      host_remote_->GetRootViewAccessibilityToken(&browser_pid, &element_token);
      [NSAccessibilityRemoteUIElement
          registerRemoteUIProcessIdentifier:browser_pid];
      remote_accessibility_element_ =
          ui::RemoteAccessibility::GetRemoteElementFromToken(element_token);
    }
    return remote_accessibility_element_;
  }
  void DispatchKeyEvent(ui::KeyEvent* event) override {
    bool event_handled = false;
    host_remote_->DispatchKeyEventRemote(std::make_unique<ui::KeyEvent>(*event),
                                         &event_handled);
    if (event_handled)
      event->SetHandled();
  }
  bool DispatchKeyEventToMenuController(ui::KeyEvent* event) override {
    bool event_swallowed = false;
    bool event_handled = false;
    host_remote_->DispatchKeyEventToMenuControllerRemote(
        std::make_unique<ui::KeyEvent>(*event), &event_swallowed,
        &event_handled);
    if (event_handled)
      event->SetHandled();
    return event_swallowed;
  }
  void GetWordAt(const gfx::Point& location_in_content,
                 bool* found_word,
                 gfx::DecoratedText* decorated_word,
                 gfx::Point* baseline_point) override {
    *found_word = false;
  }
  remote_cocoa::DragDropClient* GetDragDropClient() override {
    // Drag-drop only doesn't work across mojo yet.
    return nullptr;
  }
  ui::TextInputClient* GetTextInputClient() override {
    // Text input doesn't work across mojo yet.
    return nullptr;
  }
  bool MustPostTaskToRunModalSheetAnimation() const override { return true; }

  mojo::AssociatedRemote<mojom::NativeWidgetNSWindowHost> host_remote_;
  mojo::AssociatedRemote<mojom::TextInputHost> text_input_host_remote_;

  std::unique_ptr<NativeWidgetNSWindowBridge> bridge_;
  NSAccessibilityRemoteUIElement* __strong remote_accessibility_element_;
};

}  // namespace

// static
ApplicationBridge* ApplicationBridge::Get() {
  static base::NoDestructor<ApplicationBridge> application_bridge;
  return application_bridge.get();
}

void ApplicationBridge::BindReceiver(
    mojo::PendingAssociatedReceiver<mojom::Application> receiver) {
  receiver_.Bind(std::move(receiver),
                 ui::WindowResizeHelperMac::Get()->task_runner());
}

void ApplicationBridge::SetContentNSViewCreateCallbacks(
    RenderWidgetHostNSViewCreateCallback render_widget_host_create_callback,
    WebContentsNSViewCreateCallback web_contents_create_callback) {
  render_widget_host_create_callback_ = render_widget_host_create_callback;
  web_contents_create_callback_ = web_contents_create_callback;
}

void ApplicationBridge::CreateAlert(
    mojo::PendingReceiver<mojom::AlertBridge> bridge_receiver) {
  // The resulting object manages its own lifetime.
  std::ignore = new AlertBridge(std::move(bridge_receiver));
}

void ApplicationBridge::CreateNativeWidgetNSWindow(
    uint64_t bridge_id,
    mojo::PendingAssociatedReceiver<mojom::NativeWidgetNSWindow>
        bridge_receiver,
    mojo::PendingAssociatedRemote<mojom::NativeWidgetNSWindowHost> host,
    mojo::PendingAssociatedRemote<mojom::TextInputHost> text_input_host) {
  // The resulting object will be destroyed when its message pipe is closed.
  std::ignore =
      new NativeWidgetBridgeOwner(bridge_id, std::move(bridge_receiver),
                                  std::move(host), std::move(text_input_host));
}

void ApplicationBridge::CreateRenderWidgetHostNSView(
    uint64_t view_id,
    mojo::PendingAssociatedRemote<mojom::StubInterface> host,
    mojo::PendingAssociatedReceiver<mojom::StubInterface> view_receiver) {
  if (!render_widget_host_create_callback_)
    return;
  render_widget_host_create_callback_.Run(view_id, host.PassHandle(),
                                          view_receiver.PassHandle());
}

void ApplicationBridge::CreateSystemMediaControlsBridge(
    mojo::PendingReceiver<system_media_controls::mojom::SystemMediaControls>
        receiver,
    mojo::PendingRemote<
        system_media_controls::mojom::SystemMediaControlsObserver> host) {
  if (!system_media_controls_bridge_) {
    system_media_controls_bridge_ =
        std::make_unique<system_media_controls::SystemMediaControlsBridge>(
            std::move(receiver), std::move(host));
  } else {
    // It's possible that ApplicationBridge is asked to make an SMCBridge for an
    // App when one has already been made. This is the case for duplicate PWAs,
    // ie. when a user has 2 of the same PWA open, and plays audio in both.
    // In that case, we just need to rebind the mojo connections.
    system_media_controls_bridge_->BindMojoConnections(std::move(receiver),
                                                       std::move(host));
  }
}

void ApplicationBridge::CreateWebContentsNSView(
    uint64_t view_id,
    mojo::PendingAssociatedRemote<mojom::StubInterface> host,
    mojo::PendingAssociatedReceiver<mojom::StubInterface> view_receiver) {
  if (!web_contents_create_callback_) {
    return;
  }
  web_contents_create_callback_.Run(view_id, host.PassHandle(),
                                    view_receiver.PassHandle());
}

void ApplicationBridge::ForwardCutCopyPaste(
    mojom::CutCopyPasteCommand command) {
  ForwardCutCopyPasteToNSApp(command);
}

// static
void ApplicationBridge::ForwardCutCopyPasteToNSApp(
    mojom::CutCopyPasteCommand command) {
  switch (command) {
    case mojom::CutCopyPasteCommand::kCut:
      [NSApp sendAction:@selector(cut:) to:nil from:nil];
      break;
    case mojom::CutCopyPasteCommand::kCopy:
      [NSApp sendAction:@selector(copy:) to:nil from:nil];
      break;
    case mojom::CutCopyPasteCommand::kPaste:
      [NSApp sendAction:@selector(paste:) to:nil from:nil];
      break;
  }
}

ApplicationBridge::ApplicationBridge() = default;

ApplicationBridge::~ApplicationBridge() = default;

}  // namespace remote_cocoa
