// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_BROWSER_WINDOW_H_
#define COMPONENTS_REMOTE_COCOA_BROWSER_WINDOW_H_

#include "base/memory/raw_ptr.h"
#include "components/remote_cocoa/browser/remote_cocoa_browser_export.h"
#include "ui/gfx/native_widget_types.h"

namespace remote_cocoa {
namespace mojom {
class NativeWidgetNSWindow;
}  // namespace mojom
class ApplicationHost;
class NativeWidgetNSWindowBridge;

// A class used to look up associated structures from a gfx::NativeWindow in the
// browser process.
class REMOTE_COCOA_BROWSER_EXPORT ScopedNativeWindowMapping {
 public:
  ScopedNativeWindowMapping(
      gfx::NativeWindow native_window,
      ApplicationHost* app_host,
      NativeWidgetNSWindowBridge* in_process_window_bridge,
      mojom::NativeWidgetNSWindow* mojo_interface);
  ~ScopedNativeWindowMapping();

  ApplicationHost* application_host() const { return application_host_; }
  NativeWidgetNSWindowBridge* in_process_window_bridge() const {
    return in_process_window_bridge_;
  }
  mojom::NativeWidgetNSWindow* mojo_interface() const {
    return mojo_interface_;
  }

 private:
  const gfx::NativeWindow native_window_;
  const raw_ptr<ApplicationHost> application_host_;
  const raw_ptr<NativeWidgetNSWindowBridge> in_process_window_bridge_;
  const raw_ptr<mojom::NativeWidgetNSWindow> mojo_interface_;
};

// Return the application host for the specified browser-side gfx::NativeWindow.
// Is non-nullptr for all views::Widget-backed NSWindows.
ApplicationHost* REMOTE_COCOA_BROWSER_EXPORT
GetWindowApplicationHost(gfx::NativeWindow window);

// Return the mojo interface for the specified browser-side gfx::NativeWindow.
// Is non-nullptr for all views::Widget-backed NSWindows.
mojom::NativeWidgetNSWindow* REMOTE_COCOA_BROWSER_EXPORT
GetWindowMojoInterface(gfx::NativeWindow window);

// Returns true if the specified NSWindow corresponds to an NSWindow that is
// being viewed in a remote process.
bool REMOTE_COCOA_BROWSER_EXPORT IsWindowRemote(gfx::NativeWindow window);

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_BROWSER_WINDOW_H_
