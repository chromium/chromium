// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_AND_FILL_VIEW_DESKTOP_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_AND_FILL_VIEW_DESKTOP_H_

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_view.h"
#include "ui/views/widget/widget.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

class SaveAndFillDialogController;

// This class is the desktop implementation of the Save and Fill view
// container (or the "View" in MVC) and owns the widget for
// SaveAndFillDialogView.
class SaveAndFillViewDesktop : public SaveAndFillDialogView {
 public:
  SaveAndFillViewDesktop(base::WeakPtr<SaveAndFillDialogController> controller,
                         content::WebContents* web_contents);
  SaveAndFillViewDesktop(const SaveAndFillViewDesktop&) = delete;
  SaveAndFillViewDesktop& operator=(const SaveAndFillViewDesktop&) = delete;
  ~SaveAndFillViewDesktop() override;

 private:
  void OnLegalMessageLinkClicked(const GURL& url);

  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> dialog_widget_;

  base::WeakPtrFactory<SaveAndFillViewDesktop> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_AND_FILL_VIEW_DESKTOP_H_
