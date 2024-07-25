// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMMERCE_PRODUCT_SPECIFICATIONS_DISCLOSURE_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_COMMERCE_PRODUCT_SPECIFICATIONS_DISCLOSURE_DIALOG_H_

#include "ui/views/widget/widget.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace content {
class WebContents;
class BrowserContext;
}  // namespace content

namespace commerce {

class ProductSpecificationsDisclosureDialog : public ui::WebDialogDelegate {
 public:
  // Show the dialog. If there is a dialog showing right now, the existing
  // dialog will be closed and a new one will be shown.
  static void ShowDialog(content::BrowserContext* browser_context,
                         content::WebContents* web_contents);

  ProductSpecificationsDisclosureDialog(
      const ProductSpecificationsDisclosureDialog&) = delete;
  ProductSpecificationsDisclosureDialog& operator=(
      const ProductSpecificationsDisclosureDialog&) = delete;
  ~ProductSpecificationsDisclosureDialog() override;

  static ProductSpecificationsDisclosureDialog* current_instance_for_testing() {
    return current_instance_;
  }

 private:
  explicit ProductSpecificationsDisclosureDialog(
      content::WebContents* contents);

  static ProductSpecificationsDisclosureDialog* current_instance_;

  raw_ptr<views::Widget> dialog_widget_ = nullptr;
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_WEBUI_COMMERCE_PRODUCT_SPECIFICATIONS_DISCLOSURE_DIALOG_H_
