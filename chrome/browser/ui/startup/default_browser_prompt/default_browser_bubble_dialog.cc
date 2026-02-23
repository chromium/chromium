// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_bubble_dialog.h"

#include <memory>
#include <utility>

#include "base/functional/callback_forward.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace {

std::unique_ptr<views::ImageView> CreateTitleImage() {
  auto title_image = std::make_unique<views::ImageView>();
  title_image->SetImage(ui::ImageModel::FromImage(
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_PRODUCT_LOGO_64)));
  title_image->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  title_image->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(0, 0, 16, 0)));
  return title_image;
}

std::unique_ptr<views::Label> CreateTitleLabel() {
  auto title_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_BUBBLE_DIALOG_TITLE),
      views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_HEADLINE_4);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetMultiLine(true);
  title_label->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(0, 0, 8, 0)));

  return title_label;
}

std::unique_ptr<views::BoxLayoutView> CreateBodyText(bool can_pin_to_taskbar) {
  auto text_container = std::make_unique<views::BoxLayoutView>();
  text_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  text_container->SetBetweenChildSpacing(8);

  // Body text.
  auto* body_label =
      text_container->AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_BUBBLE_DIALOG_BODY),
          views::style::CONTEXT_DIALOG_BODY_TEXT,
          views::style::STYLE_SECONDARY));
  body_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  body_label->SetMultiLine(true);

  // Steps to set default.
  std::u16string str_open = l10n_util::GetStringUTF16(
      IDS_DEFAULT_BROWSER_BUBBLE_DIALOG_OPEN_SETTINGS_LABEL);
  std::u16string str_default = l10n_util::GetStringUTF16(
      IDS_DEFAULT_BROWSER_BUBBLE_DIALOG_SET_DEFAULT_LABEL);
  std::vector<size_t> offsets;
  std::u16string steps_text;

  if (can_pin_to_taskbar) {
    std::u16string str_yes =
        l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_BUBBLE_DIALOG_YES_LABEL);
    steps_text = l10n_util::GetStringFUTF16(
        IDS_DEFAULT_BROWSER_BUBBLE_DIALOG_STEPS_PINNED_COMBINED,
        {str_open, str_default, str_yes}, &offsets);
  } else {
    steps_text = l10n_util::GetStringFUTF16(
        IDS_DEFAULT_BROWSER_BUBBLE_DIALOG_STEPS_COMBINED,
        {str_open, str_default}, &offsets);
  }

  auto* steps_label =
      text_container->AddChildView(std::make_unique<views::StyledLabel>());
  steps_label->SetText(steps_text);
  steps_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Apply Bold Styles to replacements.
  auto bold_style = views::StyledLabel::RangeStyleInfo();
  bold_style.text_style = views::style::TextStyle::STYLE_EMPHASIZED;

  steps_label->AddStyleRange(
      gfx::Range(offsets[0], offsets[0] + str_open.length()), bold_style);
  steps_label->AddStyleRange(
      gfx::Range(offsets[1], offsets[1] + str_default.length()), bold_style);

  if (can_pin_to_taskbar) {
    std::u16string str_yes =
        l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_BUBBLE_DIALOG_YES_LABEL);
    steps_label->AddStyleRange(
        gfx::Range(offsets[2], offsets[2] + str_yes.length()), bold_style);
  }

  return text_container;
}

std::unique_ptr<views::BoxLayoutView> CreateDialogBody(
    bool can_pin_to_taskbar) {
  auto container = std::make_unique<views::BoxLayoutView>();
  container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  container->AddChildView(CreateTitleImage());
  container->AddChildView(CreateTitleLabel());

  container->AddChildView(CreateBodyText(can_pin_to_taskbar));

  return container;
}

std::unique_ptr<views::BoxLayoutView> CreateButtonStack(
    base::OnceClosure open_settings_callback,
    base::OnceClosure close_dialog_callback) {
  auto container = std::make_unique<views::BoxLayoutView>();
  container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  container->SetBetweenChildSpacing(8);
  container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  // Top Button: "Set later"
  auto* btn_later =
      container->AddChildView(std::make_unique<views::MdTextButton>(
          std::move(close_dialog_callback),
          l10n_util::GetStringUTF16(
              IDS_DEFAULT_BROWSER_BUBBLE_DIALOG_SET_LATER_LABEL)));
  btn_later->SetStyle(ui::ButtonStyle::kTonal);
  btn_later->SetProperty(views::kElementIdentifierKey,
                         default_browser::kBubbleDialogSetLaterButtonId);

  // Bottom Button: "Open settings"
  auto* btn_open =
      container->AddChildView(std::make_unique<views::MdTextButton>(
          std::move(open_settings_callback),
          l10n_util::GetStringUTF16(
              IDS_DEFAULT_BROWSER_BUBBLE_DIALOG_OPEN_SETTINGS_LABEL)));
  btn_open->SetStyle(ui::ButtonStyle::kProminent);
  btn_open->SetProperty(views::kElementIdentifierKey,
                        default_browser::kBubbleDialogOpenSettingsButtonId);

  return container;
}

}  // namespace

namespace default_browser {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kBubbleDialogSetLaterButtonId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kBubbleDialogOpenSettingsButtonId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kBubbleDialogId);

// Static.
std::unique_ptr<views::Widget> ShowDefaultBrowserBubbleDialog(
    views::View* anchor_view,
    bool can_pin_to_taskbar,
    base::OnceClosure on_accept,
    base::OnceClosure on_dismiss) {
  std::unique_ptr<ui::DialogModel> dialog_model =
      ui::DialogModel::Builder()
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  CreateDialogBody(can_pin_to_taskbar),
                  views::BubbleDialogModelHost::FieldType::kText))
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  CreateButtonStack(
                      /*open_settings_callback=*/std::move(on_accept),
                      /*close_dialog_callback=*/std::move(on_dismiss)),
                  views::BubbleDialogModelHost::FieldType::kControl))
          .SetElementIdentifier(kBubbleDialogId)
          .OverrideShowCloseButton(/*show_close_button=*/false)
          .DisableCloseOnDeactivate()
          .Build();

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), anchor_view, views::BubbleBorder::TOP_RIGHT);

  auto widget = base::WrapUnique(views::BubbleDialogDelegate::CreateBubble(
      std::move(bubble), views::Widget::InitParams::CLIENT_OWNS_WIDGET));

  widget->Show();

  return widget;
}

}  // namespace default_browser
