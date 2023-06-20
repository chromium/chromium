// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_security_view.h"

#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kCheckboxHeight = 32;
constexpr int kProgressBarHeight = 3;
// Num of columns in the table layout, the width of which progress bar will
// span. The 5 columns are Download Icon, Padding, Status text,
// Main Button, Subpage Icon.
constexpr int kNumColumns = 5;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DownloadBubbleSubpageAction {
  kShown = 0,
  kShownCheckbox = 1,
  kShownSecondaryButton = 2,
  kShownPrimaryButton = 3,
  kPressedBackButton = 4,
  kClosedSubpage = 5,
  kClickedCheckbox = 6,
  kPressedSecondaryButton = 7,
  kPressedPrimaryButton = 8,
  kMaxValue = kPressedPrimaryButton
};
const char kSubpageActionHistogram[] = "Download.Bubble.SubpageAction";

}  // namespace

void DownloadBubbleSecurityView::AddHeader() {
  auto* header = AddChildView(std::make_unique<views::View>());
  header->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal);
  if (!features::IsChromeRefresh2023()) {
    header->SetProperty(
        views::kMarginsKey,
        gfx::Insets(ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  }

  back_button_ =
      header->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&DownloadBubbleSecurityView::BackButtonPressed,
                              base::Unretained(this)),
          features::IsChromeRefresh2023()
              ? vector_icons::kArrowBackChromeRefreshIcon
              : vector_icons::kArrowBackIcon,
          GetLayoutConstant(DOWNLOAD_ICON_SIZE)));
  views::InstallCircleHighlightPathGenerator(back_button_);
  back_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_BACK_RECENT_DOWNLOADS));
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
  if (features::IsChromeRefresh2023()) {
    title_->SetTextStyle(views::style::STYLE_HEADLINE_4);
  }

  auto* close_button =
      header->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&DownloadBubbleSecurityView::CloseBubble,
                              base::Unretained(this)),
          features::IsChromeRefresh2023()
              ? vector_icons::kCloseChromeRefreshIcon
              : vector_icons::kCloseRoundedIcon,
          GetLayoutConstant(DOWNLOAD_ICON_SIZE)));
  close_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_CLOSE));
  InstallCircleHighlightPathGenerator(close_button);
  close_button->SetProperty(views::kCrossAxisAlignmentKey,
                            views::LayoutAlignment::kStart);
}

void DownloadBubbleSecurityView::BackButtonPressed() {
  DownloadItemWarningData::AddWarningActionEvent(
      model_->GetDownloadItem(),
      DownloadItemWarningData::WarningSurface::BUBBLE_SUBPAGE,
      DownloadItemWarningData::WarningAction::BACK);
  did_log_action_ = true;
  navigation_handler_->OpenPrimaryDialog();
  base::UmaHistogramEnumeration(
      kSubpageActionHistogram, DownloadBubbleSubpageAction::kPressedBackButton);
}

void DownloadBubbleSecurityView::UpdateHeader() {
  title_->SetText(model_->GetFileNameToReportUser().LossyDisplayName());
}

void DownloadBubbleSecurityView::CloseBubble() {
  DownloadItemWarningData::AddWarningActionEvent(
      model_->GetDownloadItem(),
      DownloadItemWarningData::WarningSurface::BUBBLE_SUBPAGE,
      DownloadItemWarningData::WarningAction::CLOSE);
  did_log_action_ = true;
  // CloseDialog will delete the object. Do not access any members below.
  navigation_handler_->CloseDialog(
      views::Widget::ClosedReason::kCloseButtonClicked);
  base::UmaHistogramEnumeration(kSubpageActionHistogram,
                                DownloadBubbleSubpageAction::kClosedSubpage);
}

void DownloadBubbleSecurityView::OnCheckboxClicked() {
  DCHECK(secondary_button_);
  secondary_button_->SetEnabled(checkbox_->GetChecked());
  base::UmaHistogramEnumeration(kSubpageActionHistogram,
                                DownloadBubbleSubpageAction::kClickedCheckbox);
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
  styled_label_->SizeToFit(GetMinimumLabelWidth());

  checkbox_->SetVisible(ui_info.HasCheckbox());
  if (ui_info.HasCheckbox()) {
    base::UmaHistogramEnumeration(kSubpageActionHistogram,
                                  DownloadBubbleSubpageAction::kShownCheckbox);
    checkbox_->SetChecked(false);
    checkbox_->SetText(ui_info.checkbox_label);
  }

  if (model_->GetDangerType() == download::DownloadDangerType::
                                     DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING &&
      base::FeatureList::IsEnabled(safe_browsing::kDeepScanningUpdatedUX)) {
    size_t link_offset;
    std::u16string link_text = l10n_util::GetStringUTF16(
        IDS_DOWNLOAD_BUBBLE_SUBPAGE_DEEP_SCANNING_LINK);
    std::u16string link_label_text = l10n_util::GetStringFUTF16(
        IDS_DOWNLOAD_BUBBLE_SUBPAGE_DEEP_SCANNING_LINK_WRAPPER, link_text,
        &link_offset);
    deep_scanning_link_->SetText(link_label_text);

    gfx::Range link_range(link_offset, link_offset + link_text.length());
    views::StyledLabel::RangeStyleInfo link_style =
        views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
            &DownloadBubbleUIController::ProcessDownloadButtonPress,
            bubble_controller_, model_.get(),
            DownloadCommands::LEARN_MORE_SCANNING, /*is_main_view=*/false));
    deep_scanning_link_->AddStyleRange(link_range, link_style);
    deep_scanning_link_->SetVisible(true);
    deep_scanning_link_->SizeToFit(GetMinimumLabelWidth());
  } else {
    deep_scanning_link_->SetVisible(false);
  }
}

void DownloadBubbleSecurityView::UpdateSecondaryIconAndText() {
  DownloadUIModel::BubbleUIInfo& ui_info = download_row_view_->ui_info();

  if (ui_info.warning_secondary_text.empty()) {
    return;
  }

  secondary_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      *ui_info.warning_secondary_icon, ui::kColorSecondaryForeground,
      GetLayoutConstant(DOWNLOAD_ICON_SIZE)));

  secondary_styled_label_->SetText(ui_info.warning_secondary_text);
  // The label defaults to a single line, which would force the dialog wider;
  // instead give it a width that's the minimum we want it to have. Then the
  // Layout will stretch it back out into any additional space available.
  secondary_styled_label_->SizeToFit(GetMinimumLabelWidth());
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
  icon_text_row->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(
          side_margin,
          // In CR2023 the horizontal margin is added to the parent view.
          features::IsChromeRefresh2023() ? 0 : side_margin));

  icon_ = icon_text_row->AddChildView(std::make_unique<views::ImageView>());
  icon_->SetProperty(views::kMarginsKey, GetLayoutInsets(DOWNLOAD_ICON));
  const int icon_size = GetLayoutConstant(DOWNLOAD_ICON_SIZE);
  icon_->SetImageSize({icon_size, icon_size});

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
  if (features::IsChromeRefresh2023()) {
    styled_label_->SetDefaultTextStyle(views::style::STYLE_BODY_3);
    // Align the centers of icon and the first line of label.
    styled_label_->SetProperty(
        views::kMarginsKey,
        gfx::Insets().set_top(icon_size / 2 +
                              GetLayoutInsets(DOWNLOAD_ICON).top() -
                              styled_label_->GetLineHeight() / 2));
  }

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

  deep_scanning_link_ =
      wrapper->AddChildView(std::make_unique<views::StyledLabel>());
  deep_scanning_link_->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  deep_scanning_link_->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
}

void DownloadBubbleSecurityView::AddSecondaryIconAndText() {
  const int side_margin = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  auto* icon_text_row = AddChildView(std::make_unique<views::View>());
  icon_text_row->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  icon_text_row->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(side_margin,
                      // In CR2023 the horizontal margin is added to the
                      // parent view.
                      features::IsChromeRefresh2023() ? 0 : side_margin));

  secondary_icon_ =
      icon_text_row->AddChildView(std::make_unique<views::ImageView>());
  secondary_icon_->SetProperty(views::kMarginsKey,
                               GetLayoutInsets(DOWNLOAD_ICON));

  auto* wrapper = icon_text_row->AddChildView(std::make_unique<views::View>());
  wrapper->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  wrapper->SetProperty(views::kMarginsKey,
                       gfx::Insets().set_left(icon_label_spacing));
  wrapper->SetProperty(views::kCrossAxisAlignmentKey,
                       views::LayoutAlignment::kStretch);
  wrapper->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true));

  secondary_styled_label_ =
      wrapper->AddChildView(std::make_unique<views::StyledLabel>());
  secondary_styled_label_->SetProperty(views::kCrossAxisAlignmentKey,
                                       views::LayoutAlignment::kStretch);
  secondary_styled_label_->SetTextContext(
      views::style::CONTEXT_DIALOG_BODY_TEXT);
  secondary_styled_label_->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  secondary_styled_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true));
  if (features::IsChromeRefresh2023()) {
    secondary_styled_label_->SetDefaultTextStyle(views::style::STYLE_BODY_3);
  }
}

void DownloadBubbleSecurityView::AddProgressBar() {
  const int side_margin = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  // TODO(crbug.com/1379447): Remove the progress bar holder view here.
  // Currently the animation does not show up on deep scanning without
  // the holder.
  views::FlexLayoutView* progress_bar_holder =
      AddChildView(std::make_unique<views::FlexLayoutView>());
  progress_bar_holder->SetProperty(views::kMarginsKey,
                                   gfx::Insets(side_margin));
  progress_bar_holder->SetCanProcessEventsWithinSubtree(false);
  progress_bar_holder->SetProperty(views::kTableColAndRowSpanKey,
                                   gfx::Size(kNumColumns, 1));
  progress_bar_holder->SetProperty(views::kTableHorizAlignKey,
                                   views::LayoutAlignment::kStretch);
  progress_bar_ =
      progress_bar_holder->AddChildView(std::make_unique<views::ProgressBar>(
          /*preferred_height=*/kProgressBarHeight));
  progress_bar_->SetProperty(
      views::kMarginsKey,
      gfx::Insets().set_top(ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  progress_bar_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/false));
  // Expect to start not visible, will be updated later.
  progress_bar_->SetVisible(false);
}

void DownloadBubbleSecurityView::ProcessButtonClick(
    DownloadCommands::Command command,
    bool is_secondary_button) {
  RecordWarningActionTime(is_secondary_button);
  // First open primary dialog, and then execute the command. If a deletion
  // happens leading to closure of the bubble, it will be called after primary
  // dialog is opened.
  if (navigation_handler_ && bubble_controller_) {
    navigation_handler_->OpenPrimaryDialog();
    bubble_controller_->ProcessDownloadButtonPress(model_.get(), command,
                                                   /*is_main_view=*/false);
  }

  base::UmaHistogramEnumeration(
      kSubpageActionHistogram,
      is_secondary_button ? DownloadBubbleSubpageAction::kPressedSecondaryButton
                          : DownloadBubbleSubpageAction::kPressedPrimaryButton);
}

void DownloadBubbleSecurityView::UpdateButton(
    DownloadUIModel::BubbleUIInfo::SubpageButton button_info,
    bool is_secondary_button,
    bool has_checkbox) {
  ui::DialogButton button_type =
      is_secondary_button ? ui::DIALOG_BUTTON_CANCEL : ui::DIALOG_BUTTON_OK;

  base::OnceCallback callback(base::BindOnce(
      &DownloadBubbleSecurityView::ProcessButtonClick, base::Unretained(this),
      button_info.command, is_secondary_button));

  if (button_type == ui::DIALOG_BUTTON_CANCEL) {
    bubble_delegate_->SetCancelCallback(std::move(callback));
    bubble_delegate_->SetButtonEnabled(button_type, !has_checkbox);
    views::LabelButton* button = bubble_delegate_->GetCancelButton();
    if (button_info.color) {
      button->SetEnabledTextColorIds(*button_info.color);
    }
    secondary_button_ = button;
  } else {
    bubble_delegate_->SetAcceptCallback(std::move(callback));
  }

  bubble_delegate_->SetButtonLabel(button_type, button_info.label);
  if (button_info.is_prominent) {
    bubble_delegate_->SetDefaultButton(button_type);
  }

  base::UmaHistogramEnumeration(
      kSubpageActionHistogram,
      is_secondary_button ? DownloadBubbleSubpageAction::kShownSecondaryButton
                          : DownloadBubbleSubpageAction::kShownPrimaryButton);
}

void DownloadBubbleSecurityView::UpdateButtons() {
  bubble_delegate_->SetButtons(ui::DIALOG_BUTTON_NONE);
  bubble_delegate_->SetDefaultButton(ui::DIALOG_BUTTON_NONE);
  secondary_button_ = nullptr;
  DownloadUIModel::BubbleUIInfo& ui_info = download_row_view_->ui_info();

  if (ui_info.subpage_buttons.size() > 0) {
    bubble_delegate_->SetButtons(ui::DIALOG_BUTTON_OK);
    UpdateButton(ui_info.subpage_buttons[0], /*is_secondary_button=*/false,
                 ui_info.HasCheckbox());
  }

  if (ui_info.subpage_buttons.size() > 1) {
    bubble_delegate_->SetButtons(ui::DIALOG_BUTTON_OK |
                                 ui::DIALOG_BUTTON_CANCEL);
    UpdateButton(ui_info.subpage_buttons[1], /*is_secondary_button=*/true,
                 ui_info.HasCheckbox());
  }
}

void DownloadBubbleSecurityView::UpdateProgressBar() {
  DownloadUIModel::BubbleUIInfo& ui_info = download_row_view_->ui_info();
  // The progress bar is only supported for deep scanning currently, which
  // requires a looping progress bar.
  if (!ui_info.has_progress_bar || !ui_info.is_progress_bar_looping) {
    return;
  }

  progress_bar_->SetVisible(true);
  progress_bar_->SetValue(-1);
}

void DownloadBubbleSecurityView::RecordWarningActionTime(
    bool is_secondary_button) {
  DCHECK(warning_time_.has_value());
  // Example Histogram
  // Download.Bubble.Subpage.DangerousFile.SecondaryButtonActionTime
  std::string histogram = base::StrCat(
      {"Download.Bubble.Subpage.",
       download::GetDownloadDangerTypeString(
           model_->GetDownloadItem()->GetDangerType()),
       ".", is_secondary_button ? "Secondary" : "Primary", "ButtonActionTime"});
  base::UmaHistogramMediumTimes(histogram,
                                base::Time::Now() - (*warning_time_));
  warning_time_ = absl::nullopt;
}

void DownloadBubbleSecurityView::UpdateSecurityView(
    DownloadBubbleRowView* download_row_view) {
  warning_time_ = absl::optional<base::Time>(base::Time::Now());
  download_row_view_ = download_row_view;
  DCHECK(download_row_view_->model());
  model_ =
      DownloadItemModel::Wrap(download_row_view_->model()->GetDownloadItem());
  did_log_action_ = false;
  UpdateHeader();
  UpdateIconAndText();
  UpdateSecondaryIconAndText();
  UpdateButtons();
  UpdateProgressBar();
  base::UmaHistogramEnumeration(kSubpageActionHistogram,
                                DownloadBubbleSubpageAction::kShown);
}

void DownloadBubbleSecurityView::UpdateAccessibilityTextAndFocus() {
  DownloadUIModel::BubbleUIInfo& ui_info = download_row_view_->ui_info();
  // Announce that the subpage was opened to inform the user about the changes
  // in the UI.
#if BUILDFLAG(IS_MAC)
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kAlert);
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
    base::WeakPtr<DownloadBubbleUIController> bubble_controller,
    base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
    views::BubbleDialogDelegate* bubble_delegate)
    : bubble_controller_(std::move(bubble_controller)),
      navigation_handler_(std::move(navigation_handler)),
      bubble_delegate_(bubble_delegate) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  if (features::IsChromeRefresh2023()) {
    SetProperty(views::kMarginsKey, GetLayoutInsets(DOWNLOAD_ROW));
  }
  AddHeader();
  AddIconAndText();
  AddSecondaryIconAndText();
  AddProgressBar();
}

DownloadBubbleSecurityView::~DownloadBubbleSecurityView() {
  // Note that security view is created before it is navigated, so |model_| can
  // be null.
  if (!did_log_action_ && model_) {
    DownloadItemWarningData::AddWarningActionEvent(
        model_->GetDownloadItem(),
        DownloadItemWarningData::WarningSurface::BUBBLE_SUBPAGE,
        DownloadItemWarningData::WarningAction::DISMISS);
  }
}

int DownloadBubbleSecurityView::GetMinimumLabelWidth() const {
  const int side_margin = GetLayoutInsets(DOWNLOAD_ROW).width();
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  const int bubble_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
  return bubble_width - side_margin - GetLayoutConstant(DOWNLOAD_ICON_SIZE) -
         GetLayoutInsets(DOWNLOAD_ICON).width() - icon_label_spacing;
}

BEGIN_METADATA(DownloadBubbleSecurityView, views::View)
END_METADATA
