// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_viewer.h"

#include "base/logging.h"
#include "components/remote_cocoa/browser/window.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"

void ShowCertificateViewerForClientAuth(content::WebContents* web_contents,
                                        gfx::NativeWindow parent,
                                        net::X509Certificate* cert) {
  // The certificate viewer on macOS uses the OS viewer rather than the Views
  // implementation (see https://crbug.com/953425), so go through a Mojo
  // interface. This calls the platform APIs from the right process in PWAs.
  // See https://crbug.com/916815. If this dialog is switched to Views, the Mojo
  // call will no longer be needed.
  remote_cocoa::mojom::NativeWidgetNSWindow* mojo_window =
      remote_cocoa::GetWindowMojoInterface(parent);
  if (!mojo_window) {
    // Every WebContents window should have a Mojo interface.
    LOG(ERROR) << "Could not get window Mojo interface";
    return;
  }

  mojo_window->ShowCertificateViewer(cert);
}
