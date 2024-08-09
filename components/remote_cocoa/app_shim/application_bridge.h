// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_APPLICATION_BRIDGE_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_APPLICATION_BRIDGE_H_

#include "base/no_destructor.h"
#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"
#include "components/remote_cocoa/common/alert.mojom.h"
#include "components/remote_cocoa/common/application.mojom.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "components/remote_cocoa/common/native_widget_ns_window_host.mojom.h"
#include "components/system_media_controls/mac/remote_cocoa/system_media_controls.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace system_media_controls {
class SystemMediaControlsBridge;
}  // namespace system_media_controls

namespace remote_cocoa {

// This class implements the remote_cocoa::mojom::Application interface, and
// bridges that C++ interface to the Objective-C NSApplication. This class is
// the root of all other remote_cocoa classes (e.g, NSAlerts, NSWindows,
// NSViews).
class REMOTE_COCOA_APP_SHIM_EXPORT ApplicationBridge
    : public mojom::Application {
 public:
  static ApplicationBridge* Get();
  void BindReceiver(
      mojo::PendingAssociatedReceiver<mojom::Application> receiver);

  // Set callbacks to create content types (content types cannot be created
  // in remote_cocoa).
  // TODO(crbug.com/40595042): Move these types from content to
  // remote_cocoa.
  using RenderWidgetHostNSViewCreateCallback = base::RepeatingCallback<void(
      uint64_t view_id,
      mojo::ScopedInterfaceEndpointHandle host_handle,
      mojo::ScopedInterfaceEndpointHandle view_request_handle)>;
  using WebContentsNSViewCreateCallback = base::RepeatingCallback<void(
      uint64_t view_id,
      mojo::ScopedInterfaceEndpointHandle host_handle,
      mojo::ScopedInterfaceEndpointHandle view_request_handle)>;
  void SetContentNSViewCreateCallbacks(
      RenderWidgetHostNSViewCreateCallback render_widget_host_create_callback,
      WebContentsNSViewCreateCallback web_contents_create_callback);

  // mojom::Application:
  void CreateAlert(
      mojo::PendingReceiver<mojom::AlertBridge> bridge_receiver) override;
  void CreateNativeWidgetNSWindow(
      uint64_t bridge_id,
      mojo::PendingAssociatedReceiver<mojom::NativeWidgetNSWindow>
          bridge_receiver,
      mojo::PendingAssociatedRemote<mojom::NativeWidgetNSWindowHost> host,
      mojo::PendingAssociatedRemote<mojom::TextInputHost> text_input_host)
      override;
  void CreateRenderWidgetHostNSView(
      uint64_t view_id,
      mojo::PendingAssociatedRemote<mojom::StubInterface> host,
      mojo::PendingAssociatedReceiver<mojom::StubInterface> view_receiver)
      override;
  void CreateSystemMediaControlsBridge(
      mojo::PendingReceiver<system_media_controls::mojom::SystemMediaControls>
          receiver,
      mojo::PendingRemote<
          system_media_controls::mojom::SystemMediaControlsObserver> host)
      override;
  void CreateWebContentsNSView(
      uint64_t view_id,
      mojo::PendingAssociatedRemote<mojom::StubInterface> host,
      mojo::PendingAssociatedReceiver<mojom::StubInterface> view_receiver)
      override;
  void ForwardCutCopyPaste(mojom::CutCopyPasteCommand command) override;

  static void ForwardCutCopyPasteToNSApp(mojom::CutCopyPasteCommand command);

 private:
  friend class base::NoDestructor<ApplicationBridge>;
  ApplicationBridge();
  ~ApplicationBridge() override;

  RenderWidgetHostNSViewCreateCallback render_widget_host_create_callback_;
  WebContentsNSViewCreateCallback web_contents_create_callback_;

  std::unique_ptr<system_media_controls::SystemMediaControlsBridge>
      system_media_controls_bridge_;

  mojo::AssociatedReceiver<mojom::Application> receiver_{this};
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_APPLICATION_BRIDGE_H_
