// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ssl_client_certificate_selector_mac.h"

#import <Cocoa/Cocoa.h>
#import <SecurityInterface/SFChooseIdentityPanel.h>
#include <objc/runtime.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ssl/ssl_client_auth_observer.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector.h"
#include "chrome/browser/ui/views/certificate_selector.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_mac.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_platform_key_mac.h"
#include "ui/base/buildflags.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/views/widget/widget_observer.h"

@interface SFChooseIdentityPanel (SystemPrivate)
// A system-private interface that dismisses a panel whose sheet was started by
// -beginSheetForWindow:modalDelegate:didEndSelector:contextInfo:identities:message:
// as though the user clicked the button identified by returnCode. Verified
// present in 10.5 through 10.12.
- (void)_dismissWithCode:(NSInteger)code;
@end

// This is the main class that runs the certificate selector panel. It's in
// Objective-C mainly because the only way to get a result out of that panel is
// a callback of a target/selector pair.
@interface SSLClientCertificateSelectorMac : NSObject

- (instancetype)
initWithBrowserContext:(const content::BrowserContext*)browserContext
       certRequestInfo:(net::SSLCertRequestInfo*)certRequestInfo
              delegate:
                  (std::unique_ptr<content::ClientCertificateDelegate>)delegate;
- (void)createForWebContents:(content::WebContents*)webContents
                 clientCerts:(net::ClientCertIdentityList)inputClientCerts;

- (void)setOverlayWindow:(views::Widget*)overlayWindow;

- (void)closeSelectorSheetWithCode:(NSModalResponse)response;

@end

// A bridge to the C++ world. It performs the two tasks of being a
// SSLClientAuthObserver and bridging to the SSL authentication system, and
// being a WidgetObserver for the overlay window so that if it is closed the
// cert selector is shut down.
class SSLClientAuthObserverCocoaBridge : public SSLClientAuthObserver,
                                         public views::WidgetObserver {
 public:
  SSLClientAuthObserverCocoaBridge(
      const content::BrowserContext* browser_context,
      net::SSLCertRequestInfo* cert_request_info,
      std::unique_ptr<content::ClientCertificateDelegate> delegate,
      SSLClientCertificateSelectorMac* controller)
      : SSLClientAuthObserver(browser_context,
                              cert_request_info,
                              std::move(delegate)),
        controller_(controller) {}

  void SetOverlayWindow(views::Widget* overlay_window) {
    overlay_window->AddObserver(this);
  }

  // SSLClientAuthObserver implementation:
  void OnCertSelectedByNotification() override {
    [controller_ closeSelectorSheetWithCode:NSModalResponseStop];
  }

  // WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override {
    // Note that the SFChooseIdentityPanel takes a reference to its delegate in
    // its -beginSheetForWindow:... method (bad SFChooseIdentityPanel!) so break
    // the retain cycle by explicitly canceling the dialog.
    [controller_ closeSelectorSheetWithCode:NSModalResponseAbort];
  }

  void OnWidgetDestroying(views::Widget* widget) override {
    widget->RemoveObserver(this);
  }

 private:
  SSLClientCertificateSelectorMac* controller_;  // weak, owns us
};

namespace {

// These Clear[Window]TableViewDataSources... functions help work around a bug
// in macOS where SFChooseIdentityPanel leaks a window and some views, including
// an NSTableView. Future events may make cause the table view to query its
// dataSource, which will have been deallocated.
//
// Note that this was originally thought to be 10.12+ but this reliably crashes
// on 10.11 (says avi@).
//
// Linking against the 10.12 SDK does not "fix" this issue, since
// NSTableView.dataSource is a "weak" reference, which in non-ARC land still
// translates to "raw pointer".
//
// See https://crbug.com/653093, https://crbug.com/750242 and rdar://29409207
// for more information.

void ClearTableViewDataSources(NSView* view) {
  if (auto table_view = base::mac::ObjCCast<NSTableView>(view)) {
    table_view.dataSource = nil;
  } else {
    for (NSView* subview in view.subviews) {
      ClearTableViewDataSources(subview);
    }
  }
}

void ClearWindowTableViewDataSources(NSWindow* window) {
  ClearTableViewDataSources(window.contentView);
}

}  // namespace

@implementation SSLClientCertificateSelectorMac {
  // The list of SecIdentityRefs offered to the user.
  base::scoped_nsobject<NSMutableArray> sec_identities_;

  // The corresponding list of ClientCertIdentities.
  net::ClientCertIdentityList cert_identities_;

  // A C++ object to bridge SSLClientAuthObserver notifications to us.
  std::unique_ptr<SSLClientAuthObserverCocoaBridge> observer_;

  base::scoped_nsobject<SFChooseIdentityPanel> panel_;

  // Invisible overlay window used to block interaction with the tab underneath.
  views::Widget* overlayWindow_;
}

- (instancetype)
initWithBrowserContext:(const content::BrowserContext*)browserContext
       certRequestInfo:(net::SSLCertRequestInfo*)certRequestInfo
              delegate:(std::unique_ptr<content::ClientCertificateDelegate>)
                           delegate {
  DCHECK(browserContext);
  DCHECK(certRequestInfo);
  if ((self = [super init])) {
    observer_ = std::make_unique<SSLClientAuthObserverCocoaBridge>(
        browserContext, certRequestInfo, std::move(delegate), self);
  }
  return self;
}

// The selector sheet ended. There are four possibilities for the return code.
//
// These two return codes are actually generated by the SFChooseIdentityPanel,
// although for testing purposes the OkAndCancelableForTesting implementation
// will also generate them to simulate the user clicking buttons.
//
// - NSModalResponseOK/Cancel: The user clicked the "OK" or "Cancel" button; the
//   SSL auth system needs to be told of this choice.
//
// These two return codes are generated by the SSLClientAuthObserverCocoaBridge
// to force the SFChooseIdentityPanel to be closed for various reasons.
//
// - NSModalResponseAbort: The user closed the owning tab; the SSL auth system
//   needs to be told of this cancellation.
// - NSModalResponseStop: The SSL auth system already has an answer; just tear
//   down the dialog.
//
// Note that there is a disagreement between the docs and the SDK header file as
// to the type of the return code. It has empirically been determined to be an
// int, not an NSInteger. rdar://45344010
- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(int)returnCode
            context:(void*)context {
  if (returnCode == NSModalResponseAbort) {
    observer_->CancelCertificateSelection();
  } else if (returnCode == NSModalResponseOK ||
             returnCode == NSModalResponseCancel) {
    net::ClientCertIdentity* cert = nullptr;
    if (returnCode == NSModalResponseOK) {
      NSUInteger index = [sec_identities_ indexOfObject:(id)[panel_ identity]];
      if (index != NSNotFound)
        cert = cert_identities_[index].get();
    }

    if (cert) {
      observer_->CertificateSelected(
          cert->certificate(),
          CreateSSLPrivateKeyForSecIdentity(cert->certificate(),
                                            cert->sec_identity_ref())
              .get());
    } else {
      observer_->CertificateSelected(nullptr, nullptr);
    }
  } else {
    DCHECK_EQ(NSModalResponseStop, returnCode);
    // Do nothing else; do not call back to the SSL auth system.
  }

  // Stop observing the SSL authentication system. In theory this isn't needed
  // as the CertificateSelected() and CancelCertificateSelection() calls both
  // implicitly call StopObserving() and the SSL auth system calls
  // StopObserving() before making the OnCertSelectedByNotification() callback.
  // However, StopObserving() is idempotent so call it out of a deep paranoia
  // born of many a dangling pointer.
  observer_->StopObserving();

  // See comment at definition; this works around a bug.
  ClearWindowTableViewDataSources(sheet);

  // Do not release SFChooseIdentityPanel here. Its -_okClicked: method, after
  // calling out to this method, keeps accessing its ivars, and if panel_ is the
  // last reference keeping it alive, it will crash.
  panel_.autorelease();

  overlayWindow_->Close();  // Asynchronously releases |self|.
}

- (void)createForWebContents:(content::WebContents*)webContents
                 clientCerts:(net::ClientCertIdentityList)inputClientCerts {
  cert_identities_ = std::move(inputClientCerts);

  sec_identities_.reset([[NSMutableArray alloc] init]);
  for (const auto& cert : cert_identities_) {
    DCHECK(cert->sec_identity_ref());
    [sec_identities_ addObject:(id)cert->sec_identity_ref()];
  }

  // Get the message to display:
  NSString* message = l10n_util::GetNSStringF(
      IDS_CLIENT_CERT_DIALOG_TEXT,
      base::ASCIIToUTF16(
          observer_->cert_request_info()->host_and_port.ToString()));

  // Create and set up a system choose-identity panel.
  panel_.reset([[SFChooseIdentityPanel alloc] init]);
  [panel_ setInformativeText:message];
  [panel_ setDefaultButtonTitle:l10n_util::GetNSString(IDS_OK)];
  [panel_ setAlternateButtonTitle:l10n_util::GetNSString(IDS_CANCEL)];
  base::ScopedCFTypeRef<SecPolicyRef> sslPolicy;
  if (net::x509_util::CreateSSLClientPolicy(sslPolicy.InitializeInto()) ==
      noErr) {
    [panel_ setPolicies:(id)sslPolicy.get()];
  }
}

- (void)closeSelectorSheetWithCode:(NSModalResponse)response {
  // Closing the sheet using -[NSApp endSheet:] doesn't work, so use the private
  // method. If the sheet is already closed then this is a message send to nil
  // and thus a no-op.
  [panel_ _dismissWithCode:response];
}

- (void)showSheetForWindow:(NSWindow*)window {
  NSString* title = l10n_util::GetNSString(IDS_CLIENT_CERT_DIALOG_TITLE);
  [panel_ beginSheetForWindow:window
                modalDelegate:self
               didEndSelector:@selector(sheetDidEnd:returnCode:context:)
                  contextInfo:nil
                   identities:sec_identities_
                      message:title];
  observer_->StartObserving();
}

- (void)setOverlayWindow:(views::Widget*)overlayWindow {
  overlayWindow_ = overlayWindow;
  observer_->SetOverlayWindow(overlayWindow_);
}

@end

// A testing helper object to run a OnceClosure when deallocated. Attach it as
// an associated object to test for deallocation of an object without
// subclassing.
@interface DeallocClosureCaller : NSObject

- (instancetype)initWithDeallocClosure:(base::OnceClosure)deallocClosure;

@end

@implementation DeallocClosureCaller {
  base::OnceClosure deallocClosure_;
}

- (instancetype)initWithDeallocClosure:(base::OnceClosure)deallocClosure {
  if ((self = [super init])) {
    deallocClosure_ = std::move(deallocClosure);
  }
  return self;
}

- (void)dealloc {
  std::move(deallocClosure_).Run();
  [super dealloc];
}

@end

namespace {

// A fully transparent, borderless web-modal dialog used to display the
// OS-provided client certificate selector.
class SSLClientCertificateSelectorDelegate
    : public views::WidgetDelegateView,
      public chrome::OkAndCancelableForTesting {
 public:
  SSLClientCertificateSelectorDelegate(
      content::WebContents* contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<content::ClientCertificateDelegate> delegate)
      : certificate_selector_([[SSLClientCertificateSelectorMac alloc]
            initWithBrowserContext:contents->GetBrowserContext()
                   certRequestInfo:cert_request_info
                          delegate:std::move(delegate)]),
        weak_factory_(this) {
    views::Widget* overlay_window =
        constrained_window::ShowWebModalDialogWithOverlayViews(this, contents);
    [certificate_selector_ setOverlayWindow:overlay_window];
    [certificate_selector_ createForWebContents:contents
                                    clientCerts:std::move(client_certs)];
    [certificate_selector_ showSheetForWindow:overlay_window->GetNativeWindow()
                                                  .GetNativeNSWindow()];
  }

  // WidgetDelegate:
  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_CHILD; }

  // OkAndCancelableForTesting:
  void ClickOkButton() override {
    [certificate_selector_ closeSelectorSheetWithCode:NSModalResponseOK];
  }

  void ClickCancelButton() override {
    [certificate_selector_ closeSelectorSheetWithCode:NSModalResponseCancel];
  }

  void SetDeallocClosureForTesting(base::OnceClosure dealloc_closure) {
    DeallocClosureCaller* caller = [[DeallocClosureCaller alloc]
        initWithDeallocClosure:std::move(dealloc_closure)];
    // The use of the caller as the key is deliberate; nothing needs to ever
    // look it up, so it's a convenient unique value.
    objc_setAssociatedObject(certificate_selector_.get(), caller, caller,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [caller release];
  }

  base::OnceClosure GetCancellationCallback() {
    return base::BindOnce(&SSLClientCertificateSelectorDelegate::CloseSelector,
                          weak_factory_.GetWeakPtr());
  }

 private:
  void CloseSelector() {
    [certificate_selector_ closeSelectorSheetWithCode:NSModalResponseStop];
  }

  base::scoped_nsobject<SSLClientCertificateSelectorMac> certificate_selector_;

  base::WeakPtrFactory<SSLClientCertificateSelectorDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SSLClientCertificateSelectorDelegate);
};

}  // namespace

namespace chrome {

base::OnceClosure ShowSSLClientCertificateSelector(
    content::WebContents* contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Not all WebContentses can show modal dialogs.
  //
  // TODO(davidben): Move this hook to the WebContentsDelegate and only try to
  // show a dialog in Browser's implementation. https://crbug.com/456255
  if (!CertificateSelector::CanShow(contents))
    return base::OnceClosure();

  auto* selector_delegate = new SSLClientCertificateSelectorDelegate(
      contents, cert_request_info, std::move(client_certs),
      std::move(delegate));

  return selector_delegate->GetCancellationCallback();
}

OkAndCancelableForTesting* ShowSSLClientCertificateSelectorMacForTesting(
    content::WebContents* contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate,
    base::OnceClosure dealloc_closure) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* dialog_delegate = new SSLClientCertificateSelectorDelegate(
      contents, cert_request_info, std::move(client_certs),
      std::move(delegate));
  dialog_delegate->SetDeallocClosureForTesting(std::move(dealloc_closure));
  return dialog_delegate;
}

}  // namespace chrome
