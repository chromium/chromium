// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/certificate_viewer.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#import <SecurityInterface/SecurityInterface.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/notreached.h"
#include "net/cert/x509_util_apple.h"

namespace remote_cocoa {

void ShowCertificateViewerForWindow(NSWindow* owning_window,
                                    net::X509Certificate* certificate) {
  NSArray* cert_chain = base::apple::CFToNSOwnershipCast(
      net::x509_util::CreateSecCertificateArrayForX509Certificate(certificate)
          .release());
  if (!cert_chain)
    return;

  // Explicitly disable revocation checking, regardless of user preferences
  // or system settings. The behaviour of SFCertificatePanel is to call
  // SecTrustEvaluate on the certificate(s) supplied, effectively
  // duplicating the behaviour of net::X509Certificate::Verify(). However,
  // this call stalls the UI if revocation checking is enabled in the
  // Keychain preferences or if the cert may be an EV cert. By disabling
  // revocation checking, the stall is limited to the time taken for path
  // building and verification, which should be minimized due to the path
  // being provided in |certificates|. This does not affect normal
  // revocation checking from happening, which is controlled by
  // net::X509Certificate::Verify() and user preferences, but will prevent
  // the certificate viewer UI from displaying which certificate is revoked.
  // This is acceptable, as certificate revocation will still be shown in
  // the page info bubble if a certificate in the chain is actually revoked.
  base::apple::ScopedCFTypeRef<CFMutableArrayRef> policies(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  if (!policies.get()) {
    NOTREACHED();
    return;
  }
  // Add a basic X.509 policy, in order to match the behaviour of
  // SFCertificatePanel when no policies are specified.
  base::apple::ScopedCFTypeRef<SecPolicyRef> basic_policy(
      SecPolicyCreateBasicX509());
  if (!basic_policy) {
    NOTREACHED();
    return;
  }
  CFArrayAppendValue(policies, basic_policy.get());

  SFCertificatePanel* panel = [[SFCertificatePanel alloc] init];
  [panel setPolicies:base::apple::CFToNSPtrCast(policies.get())];
  [panel beginSheetForWindow:owning_window
               modalDelegate:nil
              didEndSelector:nil
                 contextInfo:nil
                certificates:cert_chain
                   showGroup:YES];
}

}  // namespace remote_cocoa
