// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog.h"

#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog_controller.h"
#include "chrome/browser/ui/views/site_data/site_data_row_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"

namespace {

class PageSpecificSiteDataDialogModelDelegate : public ui::DialogModelDelegate {
 public:
  explicit PageSpecificSiteDataDialogModelDelegate(
      content::WebContents* web_contents)
      : web_contents_(web_contents->GetWeakPtr()) {}

  void OnDialogExplicitlyClosed() {
    // Reset the dialog reference in the user data. If the dialog is opened
    // again, a new instance should be created. When the dialog is destroyed
    // because of the web contents being destroyed, no need to remove the user
    // data because it will be destroyed.
    if (web_contents_) {
      web_contents_->RemoveUserData(
          PageSpecificSiteDataDialogController::UserDataKey());
    }
  }

 private:
  base::WeakPtr<content::WebContents> web_contents_;
};

}  // namespace

// static
views::Widget* ShowPageSpecificSiteDataDialog(
    content::WebContents* web_contents) {
  auto bubble_delegate_unique =
      std::make_unique<PageSpecificSiteDataDialogModelDelegate>(web_contents);
  PageSpecificSiteDataDialogModelDelegate* bubble_delegate =
      bubble_delegate_unique.get();
  auto builder = ui::DialogModel::Builder(std::move(bubble_delegate_unique));
  builder
      .SetTitle(l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_DIALOG_TITLE))
      .SetInternalName("PageSpecificSiteDataDialog")
      .SetCloseActionCallback(base::BindOnce(
          &PageSpecificSiteDataDialogModelDelegate::OnDialogExplicitlyClosed,
          base::Unretained(bubble_delegate)));
  builder
      .AddCustomField(
          std::make_unique<views::BubbleDialogModelHost::CustomView>(
              std::make_unique<SiteDataRowView>(GURL("https://example.com")),
              views::BubbleDialogModelHost::FieldType::kMenuItem))
      .AddCustomField(
          std::make_unique<views::BubbleDialogModelHost::CustomView>(
              std::make_unique<SiteDataRowView>(GURL("https://example.com")),
              views::BubbleDialogModelHost::FieldType::kMenuItem));
  // TODO(crbug.com/1344787): Build the rest of the dialog. Add action handling.
  // Remove the dialog from WebContentsUserData when destroyed.
  return constrained_window::ShowWebModal(builder.Build(), web_contents);
}
