// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_security_view.h"

#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/download/bubble/download_bubble_controller.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/download/download_ui_model.h"
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
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
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
  header->SetProperty(
      views::kMarginsKey,
      gfx::Insets(ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  back_button_ =
      header->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&DownloadBubbleSecurityView::BackButtonPressed,
                              base::Unretained(this)),
          vector_icons::kArrowBackIcon, GetLayoutConstant(DOWNLOAD_ICON_SIZE)));
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
  const int side_margin = GetLayoutInsets(DOWNLOAD_ROW).width();
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  const int bubble_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
  const int min_label_width =
      bubble_width - side_margin - GetLayoutConstant(DOWNLOAD_ICON_SIZE) -
      GetLayoutInsets(DOWNLOAD_ICON).width() - icon_label_spacing;
  styled_label_->SizeToFit(min_label_width);

  checkbox_->SetVisible(ui_info.has_checkbox);
  if (ui_info.has_checkbox) {
    base::UmaHistogramEnumeration(kSubpageActionHistogram,
                                  DownloadBubbleSubpageAction::kShownCheckbox);
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
    bool is_secondary_button) {
  RecordWarningActionTime(is_secondary_button);
  // First open primary dialog, and then execute the command. If a deletion
  // happens leading to closure of the bubble, it will be called after primary
  // dialog is opened.
  navigation_handler_->OpenPrimaryDialog();
  bubble_controller_->ProcessDownloadButtonPress(model_.get(), command,
                                                 /*is_main_view=*/false);
  base::UmaHistogramEnumeration(
      kSubpageActionHistogram,
      is_secondary_button ? DownloadBubbleSubpageAction::kPressedSecondaryButton
                          : DownloadBubbleSubpageAction::kPressedPrimaryButton);
}

void DownloadBubbleSecurityView::UpdateButton(
    DownloadUIModel::BubbleUIInfo::SubpageButton button_info,
    bool is_secondary_button,
    bool has_checkbox,
    SkColor color) {
  ui::DialogButton button_type =
      is_secondary_button ? ui::DIALOG_BUTTON_CANCEL : ui::DIALOG_BUTTON_OK;

  base::OnceCallback callback(base::BindOnce(
      &DownloadBubbleSecurityView::ProcessButtonClick, base::Unretained(this),
      button_info.command, is_secondary_button));

  if (button_type == ui::DIALOG_BUTTON_CANCEL) {
    bubble_delegate_->SetCancelCallback(std::move(callback));
    bubble_delegate_->SetButtonEnabled(button_type, !has_checkbox);
    views::LabelButton* button = bubble_delegate_->GetCancelButton();
    button->SetEnabledTextColorReadabilityAdjustment(true);
    button->SetEnabledTextColors(color);
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
                 ui_info.has_checkbox,
                 GetColorProvider()->GetColor(
                     download_row_view_->ui_info().secondary_color));
  }

  if (ui_info.subpage_buttons.size() > 1) {
    bubble_delegate_->SetButtons(ui::DIALOG_BUTTON_OK |
                                 ui::DIALOG_BUTTON_CANCEL);
    UpdateButton(ui_info.subpage_buttons[1], /*is_secondary_button=*/true,
                 ui_info.has_checkbox,
                 GetColorProvider()->GetColor(
                     download_row_view_->ui_info().secondary_color));
  }
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
  UpdateButtons();
  base::UmaHistogramEnumeration(kSubpageActionHistogram,
                                DownloadBubbleSubpageAction::kShown);
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
    DownloadBubbleNavigationHandler* navigation_handler,
    views::BubbleDialogDelegate* bubble_delegate)
    : bubble_controller_(bubble_controller),
      navigation_handler_(navigation_handler),
      bubble_delegate_(bubble_delegate) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  AddHeader();
  AddIconAndText();
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

BEGIN_METADATA(DownloadBubbleSecurityView, views::View)
END_METADATA
