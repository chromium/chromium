// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/certificate_viewer.h"

#include <CoreFoundation/CFArray.h>
#include <Security/Security.h>
#import <SecurityInterface/SFCertificatePanel.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/notreached.h"
#include "net/cert/x509_util_ios_and_mac.h"
#include "net/cert/x509_util_mac.h"

namespace remote_cocoa {

void ShowCertificateViewerForWindow(NSWindow* owning_window,
                                    net::X509Certificate* certificate) {
  base::ScopedCFTypeRef<CFArrayRef> cert_chain(
      net::x509_util::CreateSecCertificateArrayForX509Certificate(certificate));
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
  base::ScopedCFTypeRef<CFMutableArrayRef> policies(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  if (!policies.get()) {
    NOTREACHED();
    return;
  }
  // Add a basic X.509 policy, in order to match the behaviour of
  // SFCertificatePanel when no policies are specified.
  base::ScopedCFTypeRef<SecPolicyRef> basic_policy;
  OSStatus status =
      net::x509_util::CreateBasicX509Policy(basic_policy.InitializeInto());
  if (status != noErr) {
    NOTREACHED();
    return;
  }
  CFArrayAppendValue(policies, basic_policy.get());

  status = net::x509_util::CreateRevocationPolicies(false, policies);
  if (status != noErr) {
    NOTREACHED();
    return;
  }

  SFCertificatePanel* panel = [[SFCertificatePanel alloc] init];
  [panel setPolicies:base::mac::CFToNSCast(policies.get())];
  [panel beginSheetForWindow:owning_window
               modalDelegate:nil
              didEndSelector:nil
                 contextInfo:nil
                certificates:base::mac::CFToNSCast(cert_chain.get())
                   showGroup:YES];
  // beginSheetForWindow: internally retains an extra reference to |panel| and
  // releases it when the sheet closes. Release the original reference so the
  // sheet is destroyed.
  [panel autorelease];
}

}  // namespace remote_cocoa
