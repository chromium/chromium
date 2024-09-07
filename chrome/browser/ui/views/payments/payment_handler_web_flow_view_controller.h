// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_WEB_FLOW_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_WEB_FLOW_VIEW_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/payments/payment_handler_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"
#include "components/payments/content/developer_console_logger.h"
#include "components/payments/content/payment_request_display_manager.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "url/gurl.h"

class Profile;

namespace views {
class ProgressBar;
}

namespace payments {

class PaymentRequestDialogView;
class PaymentRequestSpec;
class PaymentRequestState;

// Displays a screen in the Payment Request dialog that shows the web page at
// |target| inside a views::WebView control.
class PaymentHandlerWebFlowViewController
    : public PaymentRequestSheetController,
      public content::WebContentsDelegate,
      public content::WebContentsObserver {
 public:
  // This ctor forwards its first 3 args to PaymentRequestSheetController's
  // ctor.
  // |payment_request_web_contents| is the page that initiated the
  // PaymentRequest. It is used in two ways:
  // - Its web developer console is used to print error messages.
  // - Its WebContentModalDialogHost is lent to the payment handler for the
  //   display of modal dialogs initiated from the payment handler's web
  //   content.
  // |profile| is the browser context used to create the new payment handler
  // WebContents object that will navigate to |target|.
  // |first_navigation_complete_callback| is invoked once the payment handler
  // WebContents finishes the initial navigation to |target|.
  PaymentHandlerWebFlowViewController(
      base::WeakPtr<PaymentRequestSpec> spec,
      base::WeakPtr<PaymentRequestState> state,
      base::WeakPtr<PaymentRequestDialogView> dialog,
      content::WebContents* payment_request_web_contents,
      Profile* profile,
      GURL target,
      PaymentHandlerOpenWindowCallback first_navigation_complete_callback);
  ~PaymentHandlerWebFlowViewController() override;

 private:
  // PaymentRequestSheetController:
  std::u16string GetSheetTitle() override;
  void FillContentView(views::View* content_view) override;
  bool ShouldShowPrimaryButton() override;
  bool ShouldShowSecondaryButton() override;
  void PopulateSheetHeaderView(views::View* view) override;
  bool GetSheetId(DialogViewID* sheet_id) override;
  bool DisplayDynamicBorderForHiddenContents() override;
  bool CanContentViewBeScrollable() override;
  base::WeakPtr<PaymentRequestSheetController> GetWeakPtr() override;

  // content::WebContentsDelegate:
  void VisibleSecurityStateChanged(content::WebContents* source) override;
  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void LoadProgressChanged(double progress) override;
  void TitleWasSet(content::NavigationEntry* entry) override;

  void AbortPayment();

  // Calculates the header background based on the web contents theme, if any,
  // otherwise the Chrome theme.
  std::unique_ptr<views::Background> GetHeaderBackground(
      views::View* header_view);

  DeveloperConsoleLogger log_;
  raw_ptr<Profile> profile_;
  GURL target_;
  raw_ptr<views::ProgressBar, DanglingUntriaged> progress_bar_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> separator_ = nullptr;
  PaymentHandlerOpenWindowCallback first_navigation_complete_callback_;
  // Used to present modal dialog triggered from the payment handler web view,
  // e.g. an authenticator dialog.
  PaymentHandlerModalDialogManagerDelegate dialog_manager_delegate_;
  // A handler to handle unhandled keyboard messages coming back from the
  // renderer process.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // Must be the last member of a leaf class.
  base::WeakPtrFactory<PaymentHandlerWebFlowViewController> weak_ptr_factory_{
      this};
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_WEB_FLOW_VIEW_CONTROLLER_H_
