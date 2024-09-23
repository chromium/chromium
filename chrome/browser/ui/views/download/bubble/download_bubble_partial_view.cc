// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_partial_view.h"

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/link_fragment.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/table_layout.h"

namespace {

constexpr char kPartialBubbleVisibleHistogramName[] =
    "Download.Bubble.PartialView.VisibleTime";

// We want the checkbox to accept gestures when users click on the label text,
// like all other Chrome checkboxes. This ViewTargeterDelegate achieves that.
class CheckboxTargeter : public views::ViewTargeterDelegate {
 public:
  CheckboxTargeter() = default;
  ~CheckboxTargeter() override = default;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override {
    return true;
  }
};

class SuppressBubbleSettingRow : public views::View,
                                 public views::ViewTargeterDelegate {
  METADATA_HEADER(SuppressBubbleSettingRow, views::View)

 public:
  SuppressBubbleSettingRow(
      base::WeakPtr<Browser> browser,
      bool should_show_settings_link,
      base::WeakPtr<DownloadBubbleUIController> bubble_controller,
      base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler)
      : browser_(std::move(browser)),
        bubble_controller_(std::move(bubble_controller)),
        navigation_handler_(std::move(navigation_handler)) {
    // Because this view appears directly below the download rows, we want to
    // use the same insets for consistency.
    SetBorder(views::CreateEmptyBorder(GetLayoutInsets(DOWNLOAD_ROW)));

    SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

    const int icon_label_spacing =
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_LABEL_HORIZONTAL);
    auto* layout = SetLayoutManager(std::make_unique<views::TableLayout>());
    // Checkbox
    layout->AddColumn(views::LayoutAlignment::kCenter,
                      views::LayoutAlignment::kStart,
                      views::TableLayout::kFixedSize,
                      views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
    // Labels
    layout->AddPaddingColumn(views::TableLayout::kFixedSize, icon_label_spacing)
        .AddColumn(views::LayoutAlignment::kStretch,
                   views::LayoutAlignment::kStart, 1.0f,
                   views::TableLayout::ColumnSize::kUsePreferred, 0, 0);

    layout->AddRows(1, 1.0f);

    checkbox_ = AddChildView(std::make_unique<views::Checkbox>(
        std::u16string(),
        base::BindRepeating(&SuppressBubbleSettingRow::CheckboxClicked,
                            base::Unretained(this))));
    checkbox_->GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_SUPPRESS_PARTIAL_VIEW));
    checkbox_->SetChecked(
        !download::IsDownloadBubblePartialViewEnabled(browser_->profile()));
    auto targeter = std::make_unique<CheckboxTargeter>();
    checkbox_->SetEventTargeter(
        std::make_unique<views::ViewTargeter>(std::move(targeter)));
    gfx::Insets insets = GetLayoutInsets(DOWNLOAD_ICON);
    // The label within the checkbox will line up with `main_text` if we don't
    // provide any insets. This is different than the download row view, which
    // doesn't have a label within its icon column. So we use just the left and
    // right inset from DOWNLOAD_ICON.
    insets.set_top_bottom(0, 0);
    checkbox_->SetBorder(views::CreateEmptyBorder(insets));

    labels_wrapper_ = AddChildView(std::make_unique<views::View>());
    labels_wrapper_->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kVertical);

    auto* main_text =
        labels_wrapper_->AddChildView(std::make_unique<views::Label>(
            l10n_util::GetStringUTF16(
                IDS_DOWNLOAD_BUBBLE_SUPPRESS_PARTIAL_VIEW),
            views::style::CONTEXT_DIALOG_BODY_TEXT));
    main_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    size_t settings_offset;
    std::u16string settings_link_text = l10n_util::GetStringUTF16(
        IDS_DOWNLOAD_BUBBLE_SUPPRESS_PARTIAL_VIEW_SETTINGS_LINK);
    settings_text_ =
        labels_wrapper_->AddChildView(std::make_unique<views::StyledLabel>());
    settings_text_->SetText(l10n_util::GetStringFUTF16(
        IDS_DOWNLOAD_BUBBLE_SUPPRESS_PARTIAL_VIEW_SETTINGS_REMINDER,
        settings_link_text, &settings_offset));
    settings_text_->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
    settings_text_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    settings_text_->SetVisible(should_show_settings_link);

    views::StyledLabel::RangeStyleInfo link_style =
        views::StyledLabel::RangeStyleInfo::CreateForLink(
            base::BindRepeating(&SuppressBubbleSettingRow::SettingsLinkClicked,
                                base::Unretained(this)));
    settings_text_->AddStyleRange(
        gfx::Range(settings_offset,
                   settings_offset + settings_link_text.size()),
        link_style);
  }

  // views::ViewTargeterDelegate
  View* TargetForRect(View* root, const gfx::Rect& rect) override {
    views::View* target =
        views::ViewTargeterDelegate::TargetForRect(root, rect);
    // Links should operate as expected, but all other gestures on this view
    // should be forwarded to the checkbox.
    if (std::string_view(target->GetClassName()) ==
        std::string_view(views::LinkFragment::kViewClassName)) {
      return target;
    }

    return checkbox_;
  }

 private:
  void CheckboxClicked() {
    if (navigation_handler_) {
      download::SetDownloadBubblePartialViewEnabled(browser_->profile(),
                                                    !checkbox_->GetChecked());
      settings_text_->SetVisible(true);
    }
  }

  void SettingsLinkClicked() {
    if (bubble_controller_ && browser_) {
      chrome::ShowSettingsSubPage(browser_.get(), chrome::kDownloadsSubPage);
    }
  }

  base::WeakPtr<Browser> browser_ = nullptr;
  base::WeakPtr<DownloadBubbleUIController> bubble_controller_ = nullptr;
  base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler_ = nullptr;
  raw_ptr<views::Checkbox> checkbox_ = nullptr;
  raw_ptr<views::View> labels_wrapper_ = nullptr;
  raw_ptr<views::StyledLabel> settings_text_ = nullptr;
};

BEGIN_METADATA(SuppressBubbleSettingRow)
END_METADATA

bool ShouldShowSuppressSetting(Profile* profile, int impressions) {
  if (!download::IsDownloadBubblePartialViewControlledByPref()) {
    return false;
  }
  // Impressions have been incremented by this point, so the first
  // impression is 1.
  return download::IsDownloadBubblePartialViewEnabledDefaultPrefValue(
             profile) &&
         3 <= impressions && impressions <= 5;
}

bool ShouldShowSettingsLink(int impressions) {
  return impressions == 5;
}

void MaybeRecordImpression(Profile* profile, int impressions) {
  // Pref writes are moderately expensive and we never change behavior for 7+
  // impressions, so don't increment further.
  if (impressions > 6) {
    return;
  }

  download::SetDownloadBubblePartialViewImpressions(profile, impressions);
}

}  // namespace

DownloadBubblePartialView::DownloadBubblePartialView(
    base::WeakPtr<Browser> browser,
    base::WeakPtr<DownloadBubbleUIController> bubble_controller,
    base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
    const DownloadBubbleRowListViewInfo& info,
    base::OnceClosure on_interacted_closure)
    : on_interacted_closure_(std::move(on_interacted_closure)) {
  MaybeAddOtrInfoRow(browser.get());

  Profile* profile = browser->profile();
  const int impressions =
      download::DownloadBubblePartialViewImpressions(profile) + 1;
  int preferred_width = DefaultPreferredWidth();
  std::unique_ptr<SuppressBubbleSettingRow> setting_row;
  if (ShouldShowSuppressSetting(profile, impressions)) {
    setting_row = std::make_unique<SuppressBubbleSettingRow>(
        browser, ShouldShowSettingsLink(impressions), bubble_controller,
        navigation_handler);
    preferred_width =
        std::max(preferred_width, setting_row->GetPreferredSize().width());
  }

  last_download_completed_time_ = info.last_completed_time();

  BuildAndAddScrollView(std::move(browser), std::move(bubble_controller),
                        std::move(navigation_handler), info, preferred_width);

  if (setting_row) {
    const int separator_spacing =
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            DISTANCE_CONTENT_LIST_VERTICAL_MULTI) /
        2;
    auto separator = std::make_unique<views::Separator>();
    separator->SetProperty(views::kMarginsKey,
                           gfx::Insets::VH(separator_spacing, 0));

    AddChildView(std::move(separator));
    AddChildView(std::move(setting_row));
  }

  MaybeRecordImpression(profile, impressions);
}

DownloadBubblePartialView::~DownloadBubblePartialView() {
  LogVisibleTimeMetrics();
}

std::string_view DownloadBubblePartialView::GetVisibleTimeHistogramName()
    const {
  return kPartialBubbleVisibleHistogramName;
}

bool DownloadBubblePartialView::IsPartialView() const {
  return true;
}

void DownloadBubblePartialView::AddedToWidget() {
  auto* focus_manager = GetFocusManager();
  if (focus_manager) {
    focus_manager->AddFocusChangeListener(this);
  }

  if (last_download_completed_time_.has_value()) {
    GetWidget()->GetCompositor()->RequestSuccessfulPresentationTimeForNextFrame(
        base::BindOnce(
            [](base::Time download_completed_time_,
               const viz::FrameTimingDetails& frame_timing_details) {
              base::TimeTicks presentation_time =
                  frame_timing_details.presentation_feedback.timestamp;
              UmaHistogramTimes(
                  "Download.Bubble.DownloadCompletionToPartialViewShownLatency",
                  (presentation_time - base::TimeTicks::UnixEpoch()) -
                      (download_completed_time_ - base::Time::UnixEpoch()));
            },
            *last_download_completed_time_));
  }
}

void DownloadBubblePartialView::RemovedFromWidget() {
  auto* focus_manager = GetFocusManager();
  if (focus_manager) {
    focus_manager->RemoveFocusChangeListener(this);
  }
}

void DownloadBubblePartialView::OnInteracted() {
  if (on_interacted_closure_) {
    std::move(on_interacted_closure_).Run();
  }
}

void DownloadBubblePartialView::OnWillChangeFocus(views::View* before,
                                                  views::View* now) {
  if (now && Contains(now)) {
    OnInteracted();
  }
}

void DownloadBubblePartialView::OnMouseEntered(const ui::MouseEvent& event) {
  OnInteracted();
}

BEGIN_METADATA(DownloadBubblePartialView)
END_METADATA
