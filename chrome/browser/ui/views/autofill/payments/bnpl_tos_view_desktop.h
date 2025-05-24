// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_BNPL_TOS_VIEW_DESKTOP_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_BNPL_TOS_VIEW_DESKTOP_H_

#include "components/autofill/core/browser/ui/payments/bnpl_tos_view.h"
#include "ui/views/widget/widget.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

class BnplTosController;

// This class is the native desktop implementation of the Buy-Now-Pay-Later
// Terms of Service view container (or the "View" in MVC) and owns the widget
// for BnplTosDialogView.
class BnplTosViewDesktop : public BnplTosView {
 public:
  BnplTosViewDesktop(base::WeakPtr<BnplTosController> controller,
                     content::WebContents* web_contents);
  BnplTosViewDesktop(const BnplTosViewDesktop&) = delete;
  BnplTosViewDesktop& operator=(const BnplTosViewDesktop&) = delete;
  ~BnplTosViewDesktop() override;

 private:
  void OpenLink(const GURL& url);

  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> dialog_widget_;

  base::WeakPtrFactory<BnplTosViewDesktop> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_BNPL_TOS_VIEW_DESKTOP_H_
