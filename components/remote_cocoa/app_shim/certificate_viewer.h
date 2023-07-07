// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_CERTIFICATE_VIEWER_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_CERTIFICATE_VIEWER_H_

#import <Cocoa/Cocoa.h>

namespace net {
class X509Certificate;
}

namespace remote_cocoa {

// Shows the platform certificate viewer, displaying |certificate| and parented
// to |owning_window|.
void ShowCertificateViewerForWindow(NSWindow* owning_window,
                                    net::X509Certificate* certificate);

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_CERTIFICATE_VIEWER_H_
