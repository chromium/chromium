// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_security_view.h"

#include "chrome/browser/download/bubble/download_bubble_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kCheckboxHeight = 32;
constexpr int kBuffer = 40;
}  // namespace

void DownloadBubbleSecurityView::AddHeader() {
  auto* header = AddChildView(std::make_unique<views::View>());
  header->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal);
  header->SetProperty(
      views::kMarginsKey,
      gfx::Insets(ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  auto* back_button =
      header->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(
              &DownloadBubbleNavigationHandler::OpenPrimaryDialog,
              base::Unretained(navigation_handler_)),
          vector_icons::kArrowBackIcon, GetLayoutConstant(DOWNLOAD_ICON_SIZE)));
  views::InstallCircleHighlightPathGenerator(back_button);
  back_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  back_button->SetProperty(views::kCrossAxisAlignmentKey,
                           views::LayoutAlignment::kStart);

  auto* title = header->AddChildView(std::make_unique<views::Label>(
      model_->GetFileNameToReportUser().LossyDisplayName(),
      views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY));
  title->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true)
          .WithWeight(1));
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  title->SetProperty(views::kMarginsKey,
                     gfx::Insets::VH(0, icon_label_spacing));
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  auto* close_button =
      header->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&DownloadBubbleSecurityView::CloseBubble,
                              base::Unretained(this)),
          vector_icons::kCloseRoundedIcon,
          GetLayoutConstant(DOWNLOAD_ICON_SIZE)));
  close_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_CLOSE));
  InstallCircleHighlightPathGenerator(close_button);
  close_button->SetProperty(views::kCrossAxisAlignmentKey,
                            views::LayoutAlignment::kStart);
}

void DownloadBubbleSecurityView::CloseBubble() {
  navigation_handler_->CloseDialog(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

void DownloadBubbleSecurityView::OnCheckboxClicked() {
  first_button_->SetEnabled(checkbox_->GetChecked());
  first_button_->SetEnabledTextColors(
      GetColorProvider()->GetColor(info_.secondary_color));
}

void DownloadBubbleSecurityView::AddIconAndText() {
  const int side_margin = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  const int bubble_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH);

  auto* icon_text_row = AddChildView(std::make_unique<views::View>());
  icon_text_row->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  icon_text_row->SetProperty(views::kMarginsKey, gfx::Insets(side_margin));

  auto* icon =
      icon_text_row->AddChildView(std::make_unique<views::ImageView>());
  icon->SetProperty(views::kMarginsKey, GetLayoutInsets(DOWNLOAD_ICON));
  icon->SetImage(ui::ImageModel::FromVectorIcon(
      *info_.icon_model_override, info_.secondary_color,
      GetLayoutConstant(DOWNLOAD_ICON_SIZE)));

  auto* wrapper = icon_text_row->AddChildView(std::make_unique<views::View>());
  wrapper->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  wrapper->SetProperty(views::kMarginsKey,
                       gfx::Insets::VH(0, icon_label_spacing));
  wrapper->SetProperty(views::kCrossAxisAlignmentKey,
                       views::LayoutAlignment::kStretch);
  wrapper->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true)
          .WithWeight(1));

  auto* styled_label =
      wrapper->AddChildView(std::make_unique<views::StyledLabel>());
  styled_label->SetProperty(views::kCrossAxisAlignmentKey,
                            views::LayoutAlignment::kStretch);
  styled_label->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  styled_label->SetText(info_.warning_summary);
  styled_label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred,
                               /*adjust_height_for_width=*/true)
          .WithWeight(1));
  // The label defaults to a single line, which would force the dialog wider;
  // instead give it a width that's the minimum we want it to have. Then the
  // Layout will stretch it back out into any additional space available.
  // Reduce by extra buffer so it has space for word wrapping.
  const int min_label_width =
      bubble_width - side_margin * 4 - icon->GetImageModel().Size().width() -
      2 * GetLayoutInsets(DOWNLOAD_ICON).width() - icon_label_spacing - kBuffer;
  styled_label->SizeToFit(min_label_width);

  if (info_.has_checkbox) {
    checkbox_ = wrapper->AddChildView(std::make_unique<views::Checkbox>(
        info_.checkbox_label,
        base::BindRepeating(&DownloadBubbleSecurityView::OnCheckboxClicked,
                            base::Unretained(this))));
    checkbox_->SetMultiLine(true);
    checkbox_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                            views::DISTANCE_RELATED_CONTROL_VERTICAL),
                        0));
    checkbox_->SetProperty(views::kCrossAxisAlignmentKey,
                           views::LayoutAlignment::kStretch);
    checkbox_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kUnbounded,
                                 /*adjust_height_for_width=*/false)
            .WithWeight(1));
    // Set min height for checkbox, so that it can layout label accordingly.
    checkbox_->SetMinSize(gfx::Size(0, kCheckboxHeight));
  }
}

void DownloadBubbleSecurityView::ProcessButtonClick(
    DownloadCommands::Command command) {
  bubble_controller_->ProcessDownloadButtonPress(model_.get(), command);
  navigation_handler_->OpenPrimaryDialog();
}

void DownloadBubbleSecurityView::AddButtons() {
  auto* button_row = AddChildView(std::make_unique<views::View>());
  button_row->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kEnd);
  button_row->SetProperty(
      views::kMarginsKey,
      gfx::Insets(ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  if (info_.has_first_button) {
    first_button_ =
        button_row->AddChildView(std::make_unique<views::MdTextButton>(
            base::BindRepeating(&DownloadBubbleSecurityView::ProcessButtonClick,
                                base::Unretained(this),
                                info_.first_button_command),
            info_.first_button_label));
    first_button_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::VH(0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                               views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));

    if (info_.has_checkbox) {
      first_button_->SetEnabled(false);
    }
  }

  if (info_.has_second_button) {
    raw_ptr<views::MdTextButton> second_button =
        button_row->AddChildView(std::make_unique<views::MdTextButton>(
            base::BindRepeating(&DownloadBubbleSecurityView::ProcessButtonClick,
                                base::Unretained(this),
                                info_.second_button_command),
            info_.second_button_label));
    second_button->SetProminent(true);
  }
}

DownloadBubbleSecurityView::DownloadBubbleSecurityView(
    DownloadUIModel::DownloadUIModelPtr model,
    DownloadUIModel::BubbleUIInfo info,
    DownloadBubbleUIController* bubble_controller,
    DownloadBubbleNavigationHandler* navigation_handler)
    : model_(std::move(model)),
      info_(info),
      bubble_controller_(bubble_controller),
      navigation_handler_(navigation_handler) {
  DCHECK(model_.get());
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  AddHeader();
  AddIconAndText();
  AddButtons();
}

DownloadBubbleSecurityView::~DownloadBubbleSecurityView() = default;

BEGIN_METADATA(DownloadBubbleSecurityView, views::View)
END_METADATA
