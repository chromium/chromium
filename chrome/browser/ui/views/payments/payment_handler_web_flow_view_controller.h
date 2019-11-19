// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_WEB_FLOW_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_WEB_FLOW_VIEW_CONTROLLER_H_

#include "chrome/browser/ui/views/payments/payment_handler_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"
#include "components/payments/content/developer_console_logger.h"
#include "components/payments/content/payment_request_display_manager.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/separator.h"
#include "url/gurl.h"

class Profile;

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
      PaymentRequestSpec* spec,
      PaymentRequestState* state,
      PaymentRequestDialogView* dialog,
      content::WebContents* payment_request_web_contents,
      Profile* profile,
      GURL target,
      PaymentHandlerOpenWindowCallback first_navigation_complete_callback);
  ~PaymentHandlerWebFlowViewController() override;

 private:
  // PaymentRequestSheetController:
  base::string16 GetSheetTitle() override;
  void FillContentView(views::View* content_view) override;
  bool ShouldShowSecondaryButton() override;
  std::unique_ptr<views::View> CreateHeaderContentView(
      views::View* header_view) override;
  views::View* CreateHeaderContentSeparatorView() override;
  std::unique_ptr<views::Background> GetHeaderBackground(
      views::View* header_view) override;
  bool GetSheetId(DialogViewID* sheet_id) override;
  bool DisplayDynamicBorderForHiddenContents() override;

  // content::WebContentsDelegate:
  void VisibleSecurityStateChanged(content::WebContents* source) override;
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void LoadProgressChanged(double progress) override;
  void TitleWasSet(content::NavigationEntry* entry) override;
  void DidAttachInterstitialPage() override;

  void AbortPayment();

  DeveloperConsoleLogger log_;
  Profile* profile_;
  GURL target_;
  bool show_progress_bar_;
  std::unique_ptr<views::ProgressBar> progress_bar_;
  std::unique_ptr<views::Separator> separator_;
  PaymentHandlerOpenWindowCallback first_navigation_complete_callback_;
  base::string16 https_prefix_;
  // Used to present modal dialog triggered from the payment handler web view,
  // e.g. an authenticator dialog.
  PaymentHandlerModalDialogManagerDelegate dialog_manager_delegate_;
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_WEB_FLOW_VIEW_CONTROLLER_H_
