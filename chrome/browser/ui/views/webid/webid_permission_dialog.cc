// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/webid_permission_dialog.h"
#include <memory>

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/webid/identity_dialogs.h"
#include "chrome/grit/webid_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/window/dialog_delegate.h"

// Dimensions of the dialog itself.
constexpr int kDialogMinWidth = 512;
constexpr int kDialogHeight = 400;

// Dimension of the header.
constexpr int kHeaderHeight = 50;

constexpr int kImageWidth = kDialogMinWidth - 120;
constexpr int kImageHeight = kDialogHeight - 200;

std::unique_ptr<views::ImageView> CreateImage(int resource_id) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  auto image = std::make_unique<views::ImageView>();
  image->SetImage(rb.GetImageNamed(resource_id).AsImageSkia());
  image->SetImageSize({kImageWidth, kImageHeight});
  image->SetPreferredSize({kImageWidth, kImageHeight});
  return image;
}

std::unique_ptr<views::View> CreateInitialMessage(
    const base::string16& idp_hostname,
    const base::string16& rp_hostname) {
  auto view = std::make_unique<views::View>();
  auto* layout = view->SetLayoutManager(std::make_unique<views::FlexLayout>());
  auto insets = views::LayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT);

  layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetDefault(views::kMarginsKey, insets);

  view->AddChildView(CreateImage(IDR_WEBID_SIGN_IN));

  // TODO(majidvp): Use a localized string. http://crbug.com/1141125
  auto message1 = base::ASCIIToUTF16("Sign In with $1.");
  base::string16 primary_text =
      base::ReplaceStringPlaceholders(message1, {idp_hostname}, nullptr);
  auto primary_label = std::make_unique<views::StyledLabel>();
  primary_label->SetText(primary_text);
  primary_label->SetDefaultTextStyle(views::style::STYLE_PRIMARY);
  view->AddChildView(std::move(primary_label));

  auto message2 = base::ASCIIToUTF16(
      "By signing in with $1,\nthey will know you visited $2.");
  std::vector<base::string16> subst;
  subst.push_back(idp_hostname);
  subst.push_back(rp_hostname);

  std::vector<size_t> offsets;
  base::string16 secondary_text =
      base::ReplaceStringPlaceholders(message2, subst, &offsets);

  views::StyledLabel::RangeStyleInfo bold_style;
  bold_style.text_style = STYLE_EMPHASIZED_SECONDARY;

  auto secondary_label = std::make_unique<views::StyledLabel>();
  secondary_label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);

  secondary_label->SetText(secondary_text);
  secondary_label->AddStyleRange(
      gfx::Range{offsets[0], offsets[0] + idp_hostname.length()}, bold_style);
  secondary_label->AddStyleRange(
      gfx::Range{offsets[1], offsets[1] + rp_hostname.length()}, bold_style);
  view->AddChildView(std::move(secondary_label));
  return view;
}

std::unique_ptr<views::View> CreateTokenExchangeMessage(
    base::string16 idp_hostname,
    base::string16 rp_hostname) {
  auto view = std::make_unique<views::View>();
  auto* layout = view->SetLayoutManager(std::make_unique<views::FlexLayout>());
  auto insets = views::LayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT);

  layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetDefault(views::kMarginsKey, insets);

  view->AddChildView(CreateImage(IDR_WEBID_GLOBAL_ID_RISK));

  // TODO(majidvp): Use a localized string. http://crbug.com/1141125
  auto primary_text =
      base::ASCIIToUTF16("You might be sharing identifying information.");

  auto primary_label = std::make_unique<views::StyledLabel>();
  primary_label->SetText(primary_text);
  primary_label->SetDefaultTextStyle(views::style::STYLE_PRIMARY);
  view->AddChildView(std::move(primary_label));

  auto message2 = base::ASCIIToUTF16(
      "$1 could use your name and email provided by\n$2 to identify or track "
      "you across the web.");

  std::vector<base::string16> subst;
  subst.push_back(rp_hostname);
  subst.push_back(idp_hostname);

  std::vector<size_t> offsets;
  base::string16 secondary_text =
      base::ReplaceStringPlaceholders(message2, subst, &offsets);

  auto secondary_label = std::make_unique<views::StyledLabel>();
  secondary_label->SetText(secondary_text);
  secondary_label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);

  views::StyledLabel::RangeStyleInfo bold_style;
  bold_style.text_style = STYLE_EMPHASIZED_SECONDARY;
  secondary_label->AddStyleRange(
      gfx::Range{offsets[0], offsets[0] + rp_hostname.length()}, bold_style);
  secondary_label->AddStyleRange(
      gfx::Range{offsets[1], offsets[1] + idp_hostname.length()}, bold_style);
  view->AddChildView(std::move(secondary_label));
  return view;
}

WebIdPermissionDialog::WebIdPermissionDialog(
    content::WebContents* rp_web_contents,
    std::unique_ptr<views::View> content,
    WebIdPermissionDialog::Callback callback)
    : rp_web_contents_(rp_web_contents), callback_(std::move(callback)) {
  DCHECK(callback_);

  SetButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  SetModalType(ui::MODAL_TYPE_CHILD);
  SetShowCloseButton(true);
  set_margins(gfx::Insets());

  // TODO(majidvp): use localized strings
  SetButtonLabel(ui::DIALOG_BUTTON_OK, base::ASCIIToUTF16("Continue"));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, base::ASCIIToUTF16("Cancel"));

  auto width =
      views::LayoutProvider::Get()->GetSnappedDialogWidth(kDialogMinWidth);
  set_fixed_width(width);

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  content->SetPreferredSize({width, kDialogHeight - kHeaderHeight});
  AddChildView(std::move(content));
}

WebIdPermissionDialog::~WebIdPermissionDialog() {
  if (callback_) {
    // The dialog has closed without the user expressing an explicit
    // preference. The current request should be denied.
    std::move(callback_).Run(UserApproval::kDenied);
  }
}

void WebIdPermissionDialog::Show() {
  // ShowWebModalDialogViews takes ownership of this, by way of the
  // DeleteDelegate method.
  constrained_window::ShowWebModalDialogViews(this, rp_web_contents_);
}

bool WebIdPermissionDialog::Accept() {
  std::move(callback_).Run(UserApproval::kApproved);
  return true;
}

bool WebIdPermissionDialog::Cancel() {
  std::move(callback_).Run(UserApproval::kDenied);
  return true;
}

// Implement identity_dialogs.h functions

void ShowInitialWebIdPermissionDialog(
    content::WebContents* rp_web_contents,
    const base::string16& idp_hostname,
    const base::string16& rp_hostname,
    WebIdPermissionDialog::Callback callback) {
  auto content = CreateInitialMessage(idp_hostname, rp_hostname);
  auto* dialog = new WebIdPermissionDialog(rp_web_contents, std::move(content),
                                           std::move(callback));
  dialog->Show();
}

void ShowTokenExchangeWebIdPermissionDialog(
    content::WebContents* rp_web_contents,
    const base::string16& idp_hostname,
    const base::string16& rp_hostname,
    WebIdPermissionDialog::Callback callback) {
  auto content = CreateTokenExchangeMessage(idp_hostname, rp_hostname);
  auto* dialog = new WebIdPermissionDialog(rp_web_contents, std::move(content),
                                           std::move(callback));
  dialog->Show();
}
