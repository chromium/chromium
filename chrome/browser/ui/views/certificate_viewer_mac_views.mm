// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import <SecurityInterface/SFCertificatePanel.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#import "base/mac/scoped_nsobject.h"
#include "chrome/browser/certificate_viewer.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/remote_cocoa/browser/window.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_ios_and_mac.h"
#include "net/cert/x509_util_mac.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

@interface SFCertificatePanel (SystemPrivate)
// A system-private interface that dismisses a panel whose sheet was started by
// -beginSheetForWindow:
//        modalDelegate:
//       didEndSelector:
//          contextInfo:
//         certificates:
//            showGroup:
// as though the user clicked the button identified by returnCode. Verified
// present in 10.8.
- (void)_dismissWithCode:(NSInteger)code;
@end

@interface SSLCertificateViewerMac : NSObject

// Initializes |certificates_| with the certificate chain for a given
// certificate.
- (instancetype)initWithCertificate:(net::X509Certificate*)certificate
                     forWebContents:(content::WebContents*)webContents;

// Shows the certificate viewer as a Cocoa sheet.
- (void)showCertificateSheet:(NSWindow*)window;

// Closes the certificate viewer sheet.
- (void)closeCertificateSheet;

- (void)setOverlayWindow:(views::Widget*)overlayWindow;

// Closes the certificate viewer Cocoa sheet.
- (void)sheetDidEnd:(NSWindow*)parent
         returnCode:(NSInteger)returnCode
            context:(void*)context;
@end

@implementation SSLCertificateViewerMac {
  // The corresponding list of certificates.
  base::scoped_nsobject<NSArray> certificates_;
  base::scoped_nsobject<SFCertificatePanel> panel_;

  // Invisible overlay window used to block interaction with the tab underneath.
  views::Widget* overlayWindow_;
}

- (instancetype)initWithCertificate:(net::X509Certificate*)certificate
                     forWebContents:(content::WebContents*)webContents {
  if ((self = [super init])) {
    base::ScopedCFTypeRef<CFArrayRef> certChain(
        net::x509_util::CreateSecCertificateArrayForX509Certificate(
            certificate));
    if (!certChain)
      return self;
    NSArray* certificates = base::mac::CFToNSCast(certChain.get());
    certificates_.reset([certificates retain]);
  }

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
    return self;
  }
  // Add a basic X.509 policy, in order to match the behaviour of
  // SFCertificatePanel when no policies are specified.
  base::ScopedCFTypeRef<SecPolicyRef> basicPolicy;
  OSStatus status =
      net::x509_util::CreateBasicX509Policy(basicPolicy.InitializeInto());
  if (status != noErr) {
    NOTREACHED();
    return self;
  }
  CFArrayAppendValue(policies, basicPolicy.get());

  status = net::x509_util::CreateRevocationPolicies(false, policies);
  if (status != noErr) {
    NOTREACHED();
    return self;
  }

  panel_.reset([[SFCertificatePanel alloc] init]);
  [panel_ setPolicies:base::mac::CFToNSCast(policies.get())];
  return self;
}

- (void)showCertificateSheet:(NSWindow*)window {
  [panel_ beginSheetForWindow:window
                modalDelegate:self
               didEndSelector:@selector(sheetDidEnd:returnCode:context:)
                  contextInfo:nil
                 certificates:certificates_
                    showGroup:YES];
}

- (void)closeCertificateSheet {
  // Closing the sheet using -[NSApp endSheet:] doesn't work so use the private
  // method. If the sheet is already closed then this is a call on nil and thus
  // a no-op.
  [panel_ _dismissWithCode:NSModalResponseCancel];
}

- (void)sheetDidEnd:(NSWindow*)parent
         returnCode:(NSInteger)returnCode
            context:(void*)context {
  overlayWindow_->Close();  // Asynchronously releases |self|.
  panel_.reset();
}

- (void)setOverlayWindow:(views::Widget*)overlayWindow {
  overlayWindow_ = overlayWindow;
}

@end

namespace {

// A fully transparent, borderless web-modal dialog used to display the
// OS-provided window-modal sheet that displays certificate information.
class CertificateAnchorWidgetDelegate : public views::WidgetDelegateView {
 public:
  CertificateAnchorWidgetDelegate(content::WebContents* web_contents,
                                  net::X509Certificate* cert)
      : certificate_viewer_([[SSLCertificateViewerMac alloc]
            initWithCertificate:cert
                 forWebContents:web_contents]) {
    views::Widget* overlayWindow =
        constrained_window::ShowWebModalDialogWithOverlayViews(this,
                                                               web_contents);
    NSWindow* overlayNSWindow =
        overlayWindow->GetNativeWindow().GetNativeNSWindow();
    // TODO(https://crbug.com/913303): The certificate viewer's interface to
    // Cocoa should be wrapped in a mojo interface in order to allow
    // instantiating across processes. As a temporary solution, create a
    // transparent in-process window to the front.
    if (remote_cocoa::IsWindowRemote(overlayNSWindow)) {
      remote_views_clone_window_ =
          remote_cocoa::CreateInProcessTransparentClone(overlayNSWindow);
      overlayNSWindow = remote_views_clone_window_;
    }
    [certificate_viewer_ showCertificateSheet:overlayNSWindow];
    [certificate_viewer_ setOverlayWindow:overlayWindow];
  }

  ~CertificateAnchorWidgetDelegate() override {
    // Note that the SFCertificatePanel takes a reference to its delegate in its
    // -beginSheetForWindow:... method (bad SFCertificatePanel!) so break the
    // retain cycle by explicitly canceling the dialog.
    [certificate_viewer_ closeCertificateSheet];
    [remote_views_clone_window_ close];
  }

  // WidgetDelegate:
  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_CHILD; }

 private:
  base::scoped_nsobject<SSLCertificateViewerMac> certificate_viewer_;
  NSWindow* remote_views_clone_window_ = nil;

  DISALLOW_COPY_AND_ASSIGN(CertificateAnchorWidgetDelegate);
};

}  // namespace

void ShowCertificateViewer(content::WebContents* web_contents,
                           gfx::NativeWindow parent,
                           net::X509Certificate* cert) {
  // Shows a new widget, which owns the delegate.
  new CertificateAnchorWidgetDelegate(web_contents, cert);
}
