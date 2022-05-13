// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_security_view.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/download/bubble/download_bubble_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
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
constexpr auto kCommandToButtons = base::MakeFixedFlatMap<
    DownloadCommands::Command,
    raw_ptr<views::MdTextButton> DownloadBubbleSecurityView::*>(
    {{DownloadCommands::DISCARD, &DownloadBubbleSecurityView::discard_button_},
     {DownloadCommands::KEEP, &DownloadBubbleSecurityView::keep_button_},
     {DownloadCommands::DEEP_SCAN,
      &DownloadBubbleSecurityView::deep_scan_button_},
     {DownloadCommands::BYPASS_DEEP_SCANNING,
      &DownloadBubbleSecurityView::bypass_deep_scan_button_}});
}  // namespace

void DownloadBubbleSecurityView::AddHeader() {
  auto* header = AddChildView(std::make_unique<views::View>());
  header->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal);
  header->SetProperty(
      views::kMarginsKey,
      gfx::Insets(ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  back_button_ =
      header->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(
              &DownloadBubbleNavigationHandler::OpenPrimaryDialog,
              base::Unretained(navigation_handler_)),
          vector_icons::kArrowBackIcon, GetLayoutConstant(DOWNLOAD_ICON_SIZE)));
  views::InstallCircleHighlightPathGenerator(back_button_);
  back_button_->SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  back_button_->SetProperty(views::kCrossAxisAlignmentKey,
                            views::LayoutAlignment::kStart);

  title_ = header->AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_DIALOG_TITLE,
      views::style::STYLE_PRIMARY));
  title_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/false));
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  title_->SetProperty(views::kMarginsKey,
                      gfx::Insets::VH(0, icon_label_spacing));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

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

void DownloadBubbleSecurityView::UpdateHeader() {
  title_->SetText(download_row_view_->model()
                      ->GetFileNameToReportUser()
                      .LossyDisplayName());
}

void DownloadBubbleSecurityView::CloseBubble() {
  navigation_handler_->CloseDialog(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

void DownloadBubbleSecurityView::OnCheckboxClicked() {
  first_button_->SetEnabled(checkbox_->GetChecked());
}

void DownloadBubbleSecurityView::UpdateIconAndText() {
  DownloadUIModel::BubbleUIInfo& ui_info = download_row_view_->ui_info();
  icon_->SetImage(ui::ImageModel::FromVectorIcon(
      *(ui_info.icon_model_override), ui_info.secondary_color,
      GetLayoutConstant(DOWNLOAD_ICON_SIZE)));

  styled_label_->SetText(ui_info.warning_summary);
  // The label defaults to a single line, which would force the dialog wider;
  // instead give it a width that's the minimum we want it to have. Then the
  // Layout will stretch it back out into any additional space available.
  // The side margin is added twice, once in the bubble, and then for each
  // row view.
  const int side_margin = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  const int bubble_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
  const int min_label_width =
      bubble_width - side_margin * 4 - GetLayoutConstant(DOWNLOAD_ICON_SIZE) -
      GetLayoutInsets(DOWNLOAD_ICON).width() - icon_label_spacing;
  styled_label_->SizeToFit(min_label_width);

  checkbox_->SetVisible(ui_info.has_checkbox);
  if (ui_info.has_checkbox) {
    checkbox_->SetChecked(false);
    checkbox_->SetText(ui_info.checkbox_label);
  }
}

void DownloadBubbleSecurityView::AddIconAndText() {
  const int side_margin = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  auto* icon_text_row = AddChildView(std::make_unique<views::View>());
  icon_text_row->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  icon_text_row->SetProperty(views::kMarginsKey, gfx::Insets(side_margin));

  icon_ = icon_text_row->AddChildView(std::make_unique<views::ImageView>());
  icon_->SetProperty(views::kMarginsKey, GetLayoutInsets(DOWNLOAD_ICON));

  auto* wrapper = icon_text_row->AddChildView(std::make_unique<views::View>());
  wrapper->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  wrapper->SetProperty(views::kMarginsKey,
                       gfx::Insets::TLBR(0, icon_label_spacing, 0, 0));
  wrapper->SetProperty(views::kCrossAxisAlignmentKey,
                       views::LayoutAlignment::kStretch);
  wrapper->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true));

  styled_label_ = wrapper->AddChildView(std::make_unique<views::StyledLabel>());
  styled_label_->SetProperty(views::kCrossAxisAlignmentKey,
                             views::LayoutAlignment::kStretch);
  styled_label_->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  styled_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true));

  checkbox_ = wrapper->AddChildView(std::make_unique<views::Checkbox>(
      std::u16string(),
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
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true));
  // Set min height for checkbox, so that it can layout label accordingly.
  checkbox_->SetMinSize(gfx::Size(0, kCheckboxHeight));
}

void DownloadBubbleSecurityView::ProcessButtonClick(
    DownloadCommands::Command command,
    bool is_first_button) {
  RecordWarningActionTime(is_first_button);
  // First open primary dialog, and then execute the command. If a deletion
  // happens leading to closure of the bubble, it will be called after primary
  // dialog is opened.
  navigation_handler_->OpenPrimaryDialog();
  bubble_controller_->ProcessDownloadButtonPress(download_row_view_->model(),
                                                 command);
}

views::MdTextButton* DownloadBubbleSecurityView::GetButtonForCommand(
    DownloadCommands::Command command) {
  auto* button_iter = kCommandToButtons.find(command);
  return (button_iter != kCommandToButtons.end()) ? this->*(button_iter->second)
                                                  : nullptr;
}

void DownloadBubbleSecurityView::UpdateButtons() {
  discard_button_->SetVisible(false);
  keep_button_->SetVisible(false);
  deep_scan_button_->SetVisible(false);
  bypass_deep_scan_button_->SetVisible(false);
  DownloadUIModel::BubbleUIInfo& ui_info = download_row_view_->ui_info();

  if (ui_info.subpage_buttons.size() > 0) {
    first_button_ = GetButtonForCommand(ui_info.subpage_buttons[0].command);
    first_button_->SetText(ui_info.subpage_buttons[0].label);
    first_button_->SetProminent(ui_info.subpage_buttons[0].is_prominent);
    first_button_->SetEnabledTextColors(GetColorProvider()->GetColor(
        download_row_view_->ui_info().secondary_color));
    first_button_->SetEnabled(!ui_info.has_checkbox);
    first_button_->SetVisible(true);
  }
  if (ui_info.subpage_buttons.size() > 1) {
    views::MdTextButton* second_button =
        GetButtonForCommand(ui_info.subpage_buttons[1].command);
    second_button->SetText(ui_info.subpage_buttons[1].label);
    second_button->SetVisible(true);
    second_button->SetProminent(ui_info.subpage_buttons[1].is_prominent);
  }
}

void DownloadBubbleSecurityView::RecordWarningActionTime(bool is_first_button) {
  DCHECK(warning_time_.has_value());
  // Example Histogram
  // Download.Bubble.Subpage.DangerousFile.FirstButtonActionTime
  std::string histogram = base::StrCat(
      {"Download.Bubble.Subpage.",
       download::GetDownloadDangerTypeString(
           download_row_view_->model()->download()->GetDangerType()),
       ".", is_first_button ? "First" : "Second", "ButtonActionTime"});
  base::UmaHistogramMediumTimes(histogram,
                                base::Time::Now() - (*warning_time_));
  warning_time_ = absl::nullopt;
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

  gfx::Insets button_margin =
      gfx::Insets::VH(0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                             views::DISTANCE_RELATED_CONTROL_HORIZONTAL));

  auto add_button_for_command = [button_row, button_margin,
                                 this](DownloadCommands::Command command) {
    auto* button =
        button_row->AddChildView(std::make_unique<views::MdTextButton>(
            base::BindRepeating(&DownloadBubbleSecurityView::ProcessButtonClick,
                                base::Unretained(this), command,
                                /*is_first_button=*/true),
            std::u16string()));
    button->SetProperty(views::kMarginsKey, button_margin);
    return button;
  };

  // The buttons come in this order KEEP, DISCARD, BYPASS_DEEP_SCANNING,
  // DEEP_SCAN. Reorder buttons in runtime if required.
  keep_button_ = add_button_for_command(DownloadCommands::KEEP);
  discard_button_ = add_button_for_command(DownloadCommands::DISCARD);
  bypass_deep_scan_button_ =
      add_button_for_command(DownloadCommands::BYPASS_DEEP_SCANNING);
  deep_scan_button_ = add_button_for_command(DownloadCommands::DEEP_SCAN);
}

void DownloadBubbleSecurityView::UpdateSecurityView(
    DownloadBubbleRowView* download_row_view) {
  warning_time_ = absl::optional<base::Time>(base::Time::Now());
  download_row_view_ = download_row_view;
  DCHECK(download_row_view_->model());
  UpdateHeader();
  UpdateIconAndText();
  UpdateButtons();
}

void DownloadBubbleSecurityView::UpdateAccessibilityTextAndFocus() {
  DownloadUIModel::BubbleUIInfo& ui_info = download_row_view_->ui_info();
  // Announce that the subpage was opened to inform the user about the changes
  // in the UI.
#if BUILDFLAG(IS_MAC)
  GetViewAccessibility().OverrideName(ui_info.warning_summary);
  NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
#else
  GetViewAccessibility().AnnounceText(ui_info.warning_summary);
#endif

  // Focus the back button by default to ensure that focus is set when new
  // content is displayed.
  back_button_->RequestFocus();
}

DownloadBubbleSecurityView::DownloadBubbleSecurityView(
    DownloadBubbleUIController* bubble_controller,
    DownloadBubbleNavigationHandler* navigation_handler)
    : bubble_controller_(bubble_controller),
      navigation_handler_(navigation_handler) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  AddHeader();
  AddIconAndText();
  AddButtons();
}

DownloadBubbleSecurityView::~DownloadBubbleSecurityView() = default;

BEGIN_METADATA(DownloadBubbleSecurityView, views::View)
END_METADATA
