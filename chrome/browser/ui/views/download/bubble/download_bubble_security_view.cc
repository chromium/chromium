// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_security_view.h"

#include "base/containers/fixed_flat_map.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/ui/download/download_bubble_security_view_info.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_password_prompt_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_id.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/window/dialog_client_view.h"

namespace {
using offline_items_collection::ContentId;

constexpr int kProgressBarHeight = 3;
// Num of columns in the table layout, the width of which progress bar will
// span. The 5 columns are Download Icon, Padding, Status text,
// Main Button, Subpage Icon.
constexpr int kNumColumns = 5;
constexpr int kAfterParagraphSpacing = 8;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DownloadBubbleSubpageAction {
  kShown = 0,
  // Reserved (obsolete): kShownCheckbox = 1,
  kShownSecondaryButton = 2,
  kShownPrimaryButton = 3,
  kPressedBackButton = 4,
  kClosedSubpage = 5,
  // Reserved (obsolete): kClickedCheckbox = 6,
  kPressedSecondaryButton = 7,
  kPressedPrimaryButton = 8,
  kMaxValue = kPressedPrimaryButton
};
const char kSubpageActionHistogram[] = "Download.Bubble.SubpageAction";

// Whether we should page away from the security view and return to the primary
// view upon a download update.
bool ShouldReturnToPrimaryDialog(const DownloadBubbleSecurityViewInfo& info) {
  return info.danger_type() == download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED ||
         // The only non-terminal danger types where the security subpage view
         // shows are `DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING` and
         // `DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING`. We should then
         // return to the row view when the deep scan completes and is in a
         // state that doesn't have a security subpage. Specifically, that's
         // both safe and failed deep scans, but not scans that find malware.
         info.danger_type() ==
             download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE ||
         info.danger_type() ==
             download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED ||
         !info.HasSubpage();
}

bool HandleButtonClickWithDefaultClose(
    base::WeakPtr<DownloadBubbleSecurityView> security_view_weak,
    DownloadCommands::Command command,
    bool is_secondary_button) {
  if (!security_view_weak) {
    // Close the dialog.
    return true;
  }
  return security_view_weak->ProcessButtonClick(command, is_secondary_button);
}

}  // namespace

// This class encapsulates a piece of text broken into several paragraphs.
class ParagraphsView : public views::View {
  METADATA_HEADER(ParagraphsView, views::View)

 public:
  ParagraphsView() {
    SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kVertical)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
    SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kUnbounded,
                                 /*adjust_height_for_width=*/true));
    SetProperty(views::kCrossAxisAlignmentKey,
                views::LayoutAlignment::kStretch);
  }
  ~ParagraphsView() override = default;

  void SetText(const std::u16string& text) {
    paragraphs_.clear();
    RemoveAllChildViews();

    for (const auto& paragraph : base::SplitStringUsingSubstr(
             text, u"\n\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
      views::StyledLabel* label =
          AddChildView(std::make_unique<views::StyledLabel>());
      label->SetProperty(views::kCrossAxisAlignmentKey,
                         views::LayoutAlignment::kStretch);
      label->SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                   views::MaximumFlexSizeRule::kUnbounded,
                                   /*adjust_height_for_width=*/true));
      label->SetText(paragraph);
      paragraphs_.push_back(label);
    }

    Update();
  }

  void SetTextContext(int text_context) {
    if (text_context_ == text_context) {
      return;
    }

    text_context_ = text_context;
    Update();
  }

  void SetDefaultTextStyle(int text_style) {
    if (default_text_style_ == text_style) {
      return;
    }

    default_text_style_ = text_style;
    Update();
  }

  int GetLineHeight() {
    return views::TypographyProvider::Get().GetLineHeight(text_context_,
                                                          default_text_style_);
  }

  void SetAfterParagraph(int spacing) {
    if (after_paragraph_ == spacing) {
      return;
    }

    after_paragraph_ = spacing;
    Update();
  }

  void SizeToFit(int fixed_width) {
    if (fixed_width_ == fixed_width) {
      return;
    }

    fixed_width_ = fixed_width;
    Update();
  }

 private:
  void Update() {
    for (views::StyledLabel* label : paragraphs_) {
      label->SetTextContext(text_context_);
      label->SetDefaultTextStyle(default_text_style_);
      label->SetProperty(views::kMarginsKey,
                         gfx::Insets().set_bottom(after_paragraph_));
      label->SizeToFit(fixed_width_);
      label->PreferredSizeChanged();
    }

    if (!paragraphs_.empty()) {
      paragraphs_.back()->SetProperty(views::kMarginsKey, gfx::Insets());
      PreferredSizeChanged();
    }
  }

  int text_context_ = views::style::CONTEXT_LABEL;
  int default_text_style_ = views::style::STYLE_PRIMARY;
  int after_paragraph_ = 0;
  int fixed_width_ = 0;
  std::vector<raw_ptr<views::StyledLabel, VectorExperimental>> paragraphs_;
};

BEGIN_METADATA(ParagraphsView)
END_METADATA

bool DownloadBubbleSecurityView::IsInitialized() const {
  return info_->content_id().has_value();
}

void DownloadBubbleSecurityView::AddHeader() {
  auto* header = AddChildView(std::make_unique<views::View>());
  header->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal);

  back_button_ =
      header->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&DownloadBubbleSecurityView::BackButtonPressed,
                              base::Unretained(this)),
          vector_icons::kArrowBackChromeRefreshIcon,
          GetLayoutConstant(DOWNLOAD_ICON_SIZE)));
  views::InstallCircleHighlightPathGenerator(back_button_);
  back_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_BACK_RECENT_DOWNLOADS));
  back_button_->SetProperty(views::kCrossAxisAlignmentKey,
                            views::LayoutAlignment::kStart);

  title_ = header->AddChildView(std::make_unique<views::StyledLabel>());
  title_->SetDefaultTextStyle(views::style::STYLE_PRIMARY);
  title_->SetTextContext(views::style::CONTEXT_DIALOG_TITLE);
  title_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true));
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  title_->SetProperty(views::kMarginsKey,
                      gfx::Insets::VH(0, icon_label_spacing));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->SetDefaultTextStyle(views::style::STYLE_HEADLINE_4);

  auto* close_button =
      header->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&DownloadBubbleSecurityView::CloseBubble,
                              base::Unretained(this)),
          vector_icons::kCloseChromeRefreshIcon,
          GetLayoutConstant(DOWNLOAD_ICON_SIZE)));
  close_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_CLOSE));
  InstallCircleHighlightPathGenerator(close_button);
  close_button->SetProperty(views::kCrossAxisAlignmentKey,
                            views::LayoutAlignment::kStart);
}

void DownloadBubbleSecurityView::BackButtonPressed() {
  if (IsInitialized()) {
    delegate_->AddSecuritySubpageWarningActionEvent(
        content_id(), DownloadItemWarningData::WarningAction::BACK);
    did_log_action_ = true;
    base::UmaHistogramEnumeration(
        kSubpageActionHistogram,
        DownloadBubbleSubpageAction::kPressedBackButton);
  }
  Reset();
  navigation_handler_->OpenPrimaryDialog();
}

void DownloadBubbleSecurityView::UpdateHeader() {
  title_->SetText(info_->title_text());
  title_->SizeToFit(GetMinimumTitleWidth());
  title_->PreferredSizeChanged();
}

void DownloadBubbleSecurityView::CloseBubble() {
  if (IsInitialized()) {
    delegate_->AddSecuritySubpageWarningActionEvent(
        content_id(), DownloadItemWarningData::WarningAction::CLOSE);
    did_log_action_ = true;
  }
  // CloseDialog will delete the object. Do not access any members below.
  navigation_handler_->CloseDialog(
      views::Widget::ClosedReason::kCloseButtonClicked);
  base::UmaHistogramEnumeration(kSubpageActionHistogram,
                                DownloadBubbleSubpageAction::kClosedSubpage);
}

void DownloadBubbleSecurityView::UpdateIconAndText() {
  icon_->SetImage(ui::ImageModel::FromVectorIcon(
      *(info_->icon_model_override()), info_->secondary_color(),
      GetLayoutConstant(DOWNLOAD_ICON_SIZE)));

  paragraphs_->SetText(info_->warning_summary());

  // The label defaults to a single line, which would force the dialog wider;
  // instead give it a width that's the minimum we want it to have. Then the
  // Layout will stretch it back out into any additional space available.
  paragraphs_->SizeToFit(GetMinimumLabelWidth());

  if (info_->learn_more_link()) {
    learn_more_link_->SetText(info_->learn_more_link()->label_and_link_text);
    size_t link_start_offset =
        info_->learn_more_link()->linked_range.start_offset;
    gfx::Range link_range{
        link_start_offset,
        link_start_offset + info_->learn_more_link()->linked_range.length};
    // Unretained is safe because `delegate_` outlives this, which owns
    // `learn_more_link_`.
    views::StyledLabel::RangeStyleInfo link_style =
        views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
            &DownloadBubbleSecurityView::Delegate::
                ProcessSecuritySubpageButtonPress,
            base::Unretained(delegate_), content_id(),
            info_->learn_more_link()->linked_range.command));
    learn_more_link_->AddStyleRange(link_range, link_style);
    learn_more_link_->SetVisible(true);
    learn_more_link_->SizeToFit(GetMinimumLabelWidth());
    learn_more_link_->PreferredSizeChanged();
  } else {
    learn_more_link_->SetVisible(false);
  }
}

void DownloadBubbleSecurityView::UpdateSecondaryIconAndText() {
  secondary_icon_->SetVisible(!info_->warning_secondary_text().empty());
  secondary_styled_label_->SetVisible(!info_->warning_secondary_text().empty());

  if (info_->warning_secondary_text().empty()) {
    return;
  }

  secondary_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      *info_->warning_secondary_icon(), ui::kColorSecondaryForeground,
      GetLayoutConstant(DOWNLOAD_ICON_SIZE)));

  secondary_styled_label_->SetText(info_->warning_secondary_text());
  // The label defaults to a single line, which would force the dialog wider;
  // instead give it a width that's the minimum we want it to have. Then the
  // Layout will stretch it back out into any additional space available.
  secondary_styled_label_->SizeToFit(GetMinimumLabelWidth());
  secondary_styled_label_->PreferredSizeChanged();
}

void DownloadBubbleSecurityView::AddIconAndContents() {
  const int side_margin = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  auto* icon_text_row = AddChildView(std::make_unique<views::View>());
  icon_text_row->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  icon_text_row->SetProperty(views::kMarginsKey,
                             gfx::Insets::VH(side_margin, 0));

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

  paragraphs_ = wrapper->AddChildView(std::make_unique<ParagraphsView>());
  paragraphs_->SetProperty(views::kCrossAxisAlignmentKey,
                           views::LayoutAlignment::kStretch);
  paragraphs_->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  paragraphs_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true));
  paragraphs_->SetAfterParagraph(kAfterParagraphSpacing);
  paragraphs_->SetDefaultTextStyle(views::style::STYLE_BODY_3);
  // Align the centers of icon and the first line of label.
  paragraphs_->SetProperty(
      views::kMarginsKey,
      gfx::Insets().set_top(icon_size / 2 +
                            GetLayoutInsets(DOWNLOAD_ICON).top() -
                            paragraphs_->GetLineHeight() / 2));

  learn_more_link_ =
      wrapper->AddChildView(std::make_unique<views::StyledLabel>());
  learn_more_link_->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  learn_more_link_->SetDefaultTextStyle(views::style::STYLE_PRIMARY);
  // `learn_more_link_` is after `paragraphs_`, and we should have the
  // paragraph spacing between them.
  learn_more_link_->SetProperty(views::kMarginsKey,
                                gfx::Insets().set_top(kAfterParagraphSpacing));

  AddPasswordPrompt(wrapper);
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
  icon_text_row->SetProperty(views::kMarginsKey,
                             gfx::Insets::VH(side_margin, 0));

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
  secondary_styled_label_->SetDefaultTextStyle(views::style::STYLE_BODY_3);
}

void DownloadBubbleSecurityView::AddProgressBar() {
  const int side_margin = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  // TODO(crbug.com/40875578): Remove the progress bar holder view here.
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
      progress_bar_holder->AddChildView(std::make_unique<views::ProgressBar>());
  progress_bar_->SetPreferredHeight(kProgressBarHeight);
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

void DownloadBubbleSecurityView::AddPasswordPrompt(views::View* parent) {
  password_prompt_ = parent->AddChildView(
      std::make_unique<DownloadBubblePasswordPromptView>());
  password_prompt_->SetVisible(false);
  password_prompt_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/false));
  password_prompt_->SetProperty(
      views::kMarginsKey,
      gfx::Insets().set_top(ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  UpdatePasswordPrompt();
}

bool DownloadBubbleSecurityView::ProcessButtonClick(
    DownloadCommands::Command command,
    bool is_secondary_button) {
  if (!IsInitialized()) {
    return true;
  }
  if (!navigation_handler_) {
    // If the navigation handler has gone away, close the dialog.
    return true;
  }

  // TODO(crbug.com/40931768): Remove the special-cased DownloadCommands by
  // creating a dedicated View for local decryption prompts and deep scanning.
  if (command == DownloadCommands::DEEP_SCAN) {
    if (info_->danger_type() ==
        download::DownloadDangerType::
            DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING) {
      return ProcessLocalPasswordDecryptionClick();
    } else {
      return ProcessDeepScanClick();
    }
  }

  if (info_->danger_type() ==
      download::DownloadDangerType::
          DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING) {
    delegate_->ProcessLocalPasswordInProgressClick(content_id(), command);
    return false;
  }

  // Record metrics only if we are actually processing the command.
  RecordWarningActionTime(is_secondary_button);
  base::UmaHistogramEnumeration(
      kSubpageActionHistogram,
      is_secondary_button ? DownloadBubbleSubpageAction::kPressedSecondaryButton
                          : DownloadBubbleSubpageAction::kPressedPrimaryButton);

  // Process the command first, since this may become uninitialized once the
  // navigation occurs.
  delegate_->ProcessSecuritySubpageButtonPress(content_id(), command);
  return true;
}

void DownloadBubbleSecurityView::MaybeLogDismiss() {
  if (did_log_action_ || !IsInitialized()) {
    return;
  }
  delegate_->AddSecuritySubpageWarningActionEvent(
      content_id(), DownloadItemWarningData::WarningAction::DISMISS);
  did_log_action_ = true;
}

const offline_items_collection::ContentId&
DownloadBubbleSecurityView::content_id() const {
  CHECK(info_->content_id().has_value());
  return info_->content_id().value();
}

void DownloadBubbleSecurityView::UpdateButton(
    DownloadBubbleSecurityViewInfo::SubpageButton button_info,
    bool is_secondary_button) {
  ui::mojom::DialogButton button_type = is_secondary_button
                                            ? ui::mojom::DialogButton::kCancel
                                            : ui::mojom::DialogButton::kOk;

  base::RepeatingCallback<bool()> callback(base::BindRepeating(
      &HandleButtonClickWithDefaultClose, weak_factory_.GetWeakPtr(),
      button_info.command, is_secondary_button));

  if (button_type == ui::mojom::DialogButton::kCancel) {
    bubble_delegate_->SetCancelCallbackWithClose(callback);
    bubble_delegate_->SetButtonEnabled(button_type, true);
    views::LabelButton* button = bubble_delegate_->GetCancelButton();
    if (button_info.text_color) {
      button->SetEnabledTextColorIds(*button_info.text_color);
    }
  } else {
    bubble_delegate_->SetAcceptCallbackWithClose(callback);
  }

  bubble_delegate_->SetButtonLabel(button_type, button_info.label);
  if (button_info.is_prominent) {
    bubble_delegate_->SetDefaultButton(static_cast<int>(button_type));
  }

  base::UmaHistogramEnumeration(
      kSubpageActionHistogram,
      is_secondary_button ? DownloadBubbleSubpageAction::kShownSecondaryButton
                          : DownloadBubbleSubpageAction::kShownPrimaryButton);
}

void DownloadBubbleSecurityView::UpdateButtons() {
  bubble_delegate_->SetButtons(
      static_cast<int>(ui::mojom::DialogButton::kNone));
  bubble_delegate_->SetDefaultButton(
      static_cast<int>(ui::mojom::DialogButton::kNone));

  if (info_->has_primary_button()) {
    bubble_delegate_->SetButtons(
        static_cast<int>(ui::mojom::DialogButton::kOk));
    UpdateButton(info_->primary_button(), /*is_secondary_button=*/false);
  }

  if (info_->has_secondary_button()) {
    bubble_delegate_->SetButtons(
        static_cast<int>(ui::mojom::DialogButton::kCancel) |
        static_cast<int>(ui::mojom::DialogButton::kOk));
    UpdateButton(info_->secondary_button(), /*is_secondary_button=*/true);
  }
  // After we have updated the buttons, set the minimum width to avoid the rest
  // of the contents stretching out the dialog unnecessarily.
  bubble_delegate_->set_fixed_width(GetMinimumBubbleWidth());
  PreferredSizeChanged();
}

void DownloadBubbleSecurityView::UpdateProgressBar() {
  progress_bar_->SetVisible(info_->has_progress_bar());
  // The progress bar is only supported for deep scanning currently, which
  // requires a looping progress bar.
  if (!info_->has_progress_bar() || !info_->is_progress_bar_looping()) {
    return;
  }

  progress_bar_->SetValue(-1);
}

void DownloadBubbleSecurityView::UpdatePasswordPrompt() {
  if (!IsInitialized()) {
    return;
  }

  bool should_show = info_->danger_type() ==
                         download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING &&
                     delegate_->IsEncryptedArchive(content_id());
  should_show |=
      info_->danger_type() ==
      download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING;

  DownloadBubblePasswordPromptView::State state =
      delegate_->HasPreviousIncorrectPassword(content_id())
          ? DownloadBubblePasswordPromptView::State::kInvalid
          : DownloadBubblePasswordPromptView::State::kValid;

  password_prompt_->SetVisible(should_show);
  password_prompt_->SetState(state);
}

void DownloadBubbleSecurityView::ClearWideFields() {
  bubble_delegate_->set_fixed_width(0);
  bubble_delegate_->SetButtonLabel(ui::mojom::DialogButton::kCancel,
                                   std::u16string());
  bubble_delegate_->SetButtonLabel(ui::mojom::DialogButton::kOk,
                                   std::u16string());
  paragraphs_->SetText(std::u16string());
  // Setting an extremely low value here will force the labels to break text
  // into a large number of labels and lay them out, which is wasteful. We set a
  // conservative value of 200 here, which is small enough to ensure the bubble
  // can shrink to minimum width, but not so small as to cause performance
  // problems.
  CHECK_GE(views::LayoutProvider::Get()->GetDistanceMetric(
               views::DistanceMetric::DISTANCE_BUBBLE_PREFERRED_WIDTH),
           200);
  paragraphs_->SizeToFit(200);
  secondary_styled_label_->SetText(std::u16string());
  secondary_styled_label_->SizeToFit(200);
  secondary_styled_label_->PreferredSizeChanged();

  title_->SetText(std::u16string());
}

void DownloadBubbleSecurityView::RecordWarningActionTime(
    bool is_secondary_button) {
  DCHECK(warning_time_.has_value());
  // Example Histogram
  // Download.Bubble.Subpage.DangerousFile.SecondaryButtonActionTime
  std::string histogram = base::StrCat(
      {"Download.Bubble.Subpage.",
       download::GetDownloadDangerTypeString(info_->danger_type()), ".",
       is_secondary_button ? "Secondary" : "Primary", "ButtonActionTime"});
  base::UmaHistogramMediumTimes(histogram,
                                base::Time::Now() - (*warning_time_));
}

void DownloadBubbleSecurityView::Reset() {
  warning_time_ = std::nullopt;
}

void DownloadBubbleSecurityView::UpdateViews() {
  CHECK(IsInitialized());
  CHECK(info_->HasSubpage());

  // Our multiline labels need to know the width of the bubble in order to size
  // themselves appropriately (see `GetMinimumLabelWidth`). This means that we
  // must reset fields that increase the width of the bubble before update. This
  // avoids problems where, e.g., both the buttons and the text used to be wide
  // but aren't anymore. A single round of updates could still have a wide
  // bubble.
  ClearWideFields();
  UpdateButtons();
  UpdateHeader();
  UpdateIconAndText();
  UpdateSecondaryIconAndText();
  UpdateProgressBar();
  UpdatePasswordPrompt();
}

void DownloadBubbleSecurityView::UpdateAccessibilityTextAndFocus() {
  if (!IsInitialized()) {
    return;
  }
  // Announce that the subpage was opened to inform the user about the changes
  // in the UI.
  GetViewAccessibility().AnnounceText(info_->warning_summary());

  // Focus the back button by default to ensure that focus is set when new
  // content is displayed.
  back_button_->RequestFocus();
}

DownloadBubbleSecurityView::DownloadBubbleSecurityView(
    Delegate* delegate,
    const DownloadBubbleSecurityViewInfo& info,
    base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
    views::BubbleDialogDelegate* bubble_delegate)
    : delegate_(delegate),
      info_(info),
      navigation_handler_(std::move(navigation_handler)),
      bubble_delegate_(bubble_delegate) {
  CHECK(delegate_);
  info_->AddObserver(this);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  SetProperty(views::kMarginsKey, GetLayoutInsets(DOWNLOAD_ROW));

  AddHeader();
  AddIconAndContents();
  AddSecondaryIconAndText();
  AddProgressBar();
}

DownloadBubbleSecurityView::~DownloadBubbleSecurityView() = default;

int DownloadBubbleSecurityView::GetMinimumBubbleWidth() const {
  return ChromeLayoutProvider::Get()->GetSnappedDialogWidth(
      bubble_delegate_->GetDialogClientView()->GetMinimumSize().width());
}

int DownloadBubbleSecurityView::GetMinimumTitleWidth() const {
  // The title's width is similar to the subpage summary's width except it is
  // narrower to accommodate the close button.
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  return GetMinimumLabelWidth() - GetLayoutConstant(DOWNLOAD_ICON_SIZE) -
         icon_label_spacing;
}

int DownloadBubbleSecurityView::GetMinimumLabelWidth() const {
  const int side_margin = GetLayoutInsets(DOWNLOAD_ROW).width();
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  return GetMinimumBubbleWidth() - side_margin -
         GetLayoutConstant(DOWNLOAD_ICON_SIZE) -
         GetLayoutInsets(DOWNLOAD_ICON).width() - icon_label_spacing;
}

bool DownloadBubbleSecurityView::ProcessDeepScanClick() {
  std::optional<std::string> password;
  if (!IsInitialized()) {
    return true;
  }

  password = base::UTF16ToUTF8(password_prompt_->GetText());
  if (delegate_->IsEncryptedArchive(content_id()) && password->empty()) {
    password_prompt_->SetState(
        DownloadBubblePasswordPromptView::State::kInvalidEmpty);
    return false;
  }

  DownloadItemWarningData::DeepScanTrigger trigger =
      delegate_->IsEncryptedArchive(content_id())
          ? DownloadItemWarningData::DeepScanTrigger::
                TRIGGER_ENCRYPTED_CONSUMER_PROMPT
          : DownloadItemWarningData::DeepScanTrigger::TRIGGER_CONSUMER_PROMPT;
  delegate_->ProcessDeepScanPress(content_id(), trigger, password);
  return false;
}

bool DownloadBubbleSecurityView::ProcessLocalPasswordDecryptionClick() {
  if (!IsInitialized()) {
    return true;
  }

  std::string password = base::UTF16ToUTF8(password_prompt_->GetText());
  if (password.empty()) {
    password_prompt_->SetState(
        DownloadBubblePasswordPromptView::State::kInvalidEmpty);
    return false;
  }

  delegate_->ProcessLocalDecryptionPress(content_id(), password);
  return false;
}

void DownloadBubbleSecurityView::OnInfoChanged() {
  warning_time_ = base::Time::Now();
  // If this represents a "terminal" state of a deep scan, or if the download
  // is otherwise no longer dangerous, we return to the primary dialog. Note
  // that we want this behavior even if this is a different download, e.g.
  // user clicks on a different download via entry point external to the
  // download bubble (e.g. notification on Lacros).
  if (ShouldReturnToPrimaryDialog(info_.get())) {
    navigation_handler_->OpenPrimaryDialog();
    // No need to update views here because we're resetting and returning to
    // the primary dialog anyway.
    return;
  }
  UpdateViews();
  UpdateAccessibilityTextAndFocus();
}

void DownloadBubbleSecurityView::OnContentIdChanged() {
  Reset();
  // Reset this to false because now this represents a different instance of
  // the security dialog. This should not be reset anywhere else. We only want
  // to consider it a different instance of the dialog (and potentially log a
  // new action) when the action is performed by a user, not when the browser
  // changes the danger type (when a scan is finished, for instance).
  did_log_action_ = false;
  base::UmaHistogramEnumeration(kSubpageActionHistogram,
                                DownloadBubbleSubpageAction::kShown);
}

BEGIN_METADATA(DownloadBubbleSecurityView)
END_METADATA
