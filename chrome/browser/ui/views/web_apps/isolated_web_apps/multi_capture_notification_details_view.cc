// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/multi_capture_notification_details_view.h"

#include "base/strings/utf_string_conversions.h"
#include "base/types/expected_macros.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/window/dialog_delegate.h"

namespace multi_capture {

BEGIN_METADATA(MultiCaptureNotificationDetailsView)
END_METADATA

MultiCaptureNotificationDetailsView::MultiCaptureNotificationDetailsView(
    const std::vector<std::string>& app_names) {
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  const views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetBorder(views::CreateEmptyBorder(provider->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kControl)));

  auto* admin_icon = AddChildView(std::make_unique<views::ImageView>());
  admin_icon->SetImage(ui::ImageModel::FromVectorIcon(
      // TODO(crbug.com/424102053): Replace with finalized icon.
      vector_icons::kScreenShareIcon, ui::kColorIcon, 24));
  admin_icon->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  admin_icon->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          0, 0,
          provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL),
          0));

  auto* title_label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_MULTI_CAPTURE_DETAILS_DIALOG_HEADING),
      views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY));
  title_label->SetMultiLine(true);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          0, 0,
          provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL),
          0));

  auto* description_label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(
          IDS_MULTI_CAPTURE_DETAILS_DIALOG_NO_APP_ON_ALLOWLIST_MESSAGE),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  description_label->SetMultiLine(true);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description_label->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          0, 0,
          provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL),
          0));

  auto* app_list_container = AddChildView(std::make_unique<views::View>());
  app_list_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  for (const auto& app_name : app_names) {
    auto* app_row =
        app_list_container->AddChildView(std::make_unique<views::View>());
    app_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
        provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL)));
    app_row->SetProperty(
        views::kMarginsKey,
        gfx::Insets::VH(provider->GetDistanceMetric(
                            views::DISTANCE_RELATED_CONTROL_VERTICAL) /
                            2,
                        0));

    auto* app_icon =
        app_row->AddChildView(std::make_unique<views::ImageView>());
    app_icon->SetImage(ui::ImageModel::FromVectorIcon(
        // TODO(crbug.com/424102053): Replace with finalized icon.
        vector_icons::kScreenShareIcon, ui::kColorIconSecondary, 16));

    app_row->AddChildView(std::make_unique<views::Label>(
        base::UTF8ToUTF16(app_name), views::style::CONTEXT_LABEL));
  }
  app_list_container->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, 0,
                        provider->GetDistanceMetric(
                            views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                        0));

  auto* notification_label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(
          IDS_MULTI_CAPTURE_DETAILS_DIALOG_NO_APP_ON_ALLOWLIST_FOOTER_MESSAGE),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  notification_label->SetMultiLine(true);
  notification_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  notification_label->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          0, 0,
          provider->GetDistanceMetric(views::DISTANCE_CLOSE_BUTTON_MARGIN), 0));

  auto* button_container = AddChildView(std::make_unique<views::View>());
  button_container
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal))
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);

  auto close_button = std::make_unique<views::MdTextButton>(
      views::Button::PressedCallback(
          base::BindOnce(&MultiCaptureNotificationDetailsView::CloseWidget,
                         weak_ptr_factory_.GetWeakPtr())),
      l10n_util::GetStringUTF16(
          IDS_MULTI_CAPTURE_DETAILS_DIALOG_CLOSE_BUTTON_TEXT));
  close_button->SetStyle(ui::ButtonStyle::kProminent);
  button_container->AddChildView(std::move(close_button));
}

MultiCaptureNotificationDetailsView::~MultiCaptureNotificationDetailsView() =
    default;

void MultiCaptureNotificationDetailsView::ShowCaptureDetails(
    const std::vector<std::string>& app_names) {
  std::unique_ptr<views::DialogDelegate> delegate =
      std::make_unique<views::DialogDelegate>();
  delegate->SetContentsView(
      std::make_unique<MultiCaptureNotificationDetailsView>(app_names));
  delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

  views::DialogDelegate::CreateDialogWidget(std::move(delegate),
                                            /*context=*/gfx::NativeWindow(),
                                            /*parent=*/gfx::NativeView())
      ->Show();
}

void MultiCaptureNotificationDetailsView::CloseWidget() {
  views::Widget* widget = GetWidget();
  if (!widget) {
    return;
  }
  widget->Close();
}

}  // namespace multi_capture
