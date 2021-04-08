// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/webid_permission_view.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/webid_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"

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
    const std::u16string& idp_hostname,
    const std::u16string& rp_hostname) {
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
  std::u16string primary_text =
      base::ReplaceStringPlaceholders(message1, {idp_hostname}, nullptr);
  auto primary_label = std::make_unique<views::StyledLabel>();
  primary_label->SetText(primary_text);
  primary_label->SetDefaultTextStyle(views::style::STYLE_PRIMARY);
  view->AddChildView(std::move(primary_label));

  auto message2 = base::ASCIIToUTF16(
      "By signing in with $1,\nthey will know you visited $2.");
  std::vector<std::u16string> subst;
  subst.push_back(idp_hostname);
  subst.push_back(rp_hostname);

  std::vector<size_t> offsets;
  std::u16string secondary_text =
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
    std::u16string idp_hostname,
    std::u16string rp_hostname) {
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

  std::vector<std::u16string> subst;
  subst.push_back(rp_hostname);
  subst.push_back(idp_hostname);

  std::vector<size_t> offsets;
  std::u16string secondary_text =
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

// static
std::unique_ptr<WebIdPermissionView>
WebIdPermissionView::CreateForInitialPermission(
    WebIdDialogViews* dialog,
    const std::u16string& idp_hostname,
    const std::u16string& rp_hostname) {
  return std::make_unique<WebIdPermissionView>(
      dialog, CreateInitialMessage(idp_hostname, rp_hostname));
}

// static
std::unique_ptr<WebIdPermissionView>
WebIdPermissionView::CreateForTokenExchangePermission(
    WebIdDialogViews* dialog,
    const std::u16string& idp_hostname,
    const std::u16string& rp_hostname) {
  return std::make_unique<WebIdPermissionView>(
      dialog, CreateTokenExchangeMessage(idp_hostname, rp_hostname));
}

WebIdPermissionView::WebIdPermissionView(WebIdDialogViews* dialog,
                                         std::unique_ptr<views::View> content) {
  dialog->SetButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  dialog->SetButtonEnabled(ui::DIALOG_BUTTON_OK, true);
  dialog->SetButtonEnabled(ui::DIALOG_BUTTON_CANCEL, true);

  // TODO(majidvp): use localized strings
  dialog->SetButtonLabel(ui::DIALOG_BUTTON_OK, u"Continue");
  dialog->SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, u"Cancel");

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  content->SetPreferredSize({kDialogMinWidth, kDialogHeight - kHeaderHeight});

  AddChildView(std::move(content));
}

WebIdPermissionView::~WebIdPermissionView() = default;
