// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharesheet_bubble_view.h"

#include <memory>
#include <utility>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_metrics.h"
#include "chrome/browser/sharesheet/sharesheet_service_delegate.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace {

// Sizes are in px.
// kButtonWidth = 76px width + 2*8px for padding on left and right
constexpr int kButtonWidth = 92;
// kButtonHeight = 88px height + 2*8px for padding on top and bottom.
constexpr int kButtonHeight = 104;
constexpr int kButtonLineHeight = 20;
constexpr int kButtonPadding = 8;

constexpr int kBubbleTopPaddingFromWindow = 36;

constexpr int kCornerRadius = 12;
constexpr int kMaxTargetsPerRow = 4;
// TargetViewHeight is 2*kButtonHeight + kButtonPadding
constexpr int kTargetViewHeight = 216;
constexpr int kDefaultBubbleWidth = 416;
constexpr int kShortSpacing = 20;
constexpr int kSpacing = 24;
constexpr int kTitleLineHeight = 24;
constexpr char kTitle[] = "Share";

constexpr SkColor kShareTitleColor = gfx::kGoogleGrey900;
constexpr SkColor kShareTargetTitleColor = gfx::kGoogleGrey700;

enum { COLUMN_SET_ID_TITLE, COLUMN_SET_ID_TARGETS };

}  // namespace

// ShareSheetTargetButton

// A button that represents a candidate share target.
class ShareSheetTargetButton : public views::Button {
 public:
  ShareSheetTargetButton(views::ButtonListener* listener,
                         const base::string16& display_name,
                         const gfx::ImageSkia* icon)
      : Button(listener) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(kButtonPadding),
        kButtonPadding, true));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kStart);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    auto* image = AddChildView(std::make_unique<views::ImageView>());
    image->set_can_process_events_within_subtree(false);

    if (!icon->isNull()) {
      image->SetImage(icon);
    }

    auto* label = AddChildView(std::make_unique<views::Label>(display_name));
    label->SetFontList(gfx::FontList("Roboto, Medium, 14px"));
    label->SetLineHeight(kButtonLineHeight);
    label->SetBackgroundColor(SK_ColorTRANSPARENT);
    label->SetEnabledColor(kShareTargetTitleColor);
    label->SetHandlesTooltips(true);
    label->SetTooltipText(display_name);
    label->SetMultiLine(false);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetHorizontalAlignment(gfx::ALIGN_CENTER);

    SetFocusForPlatform();
  }

  ShareSheetTargetButton(const ShareSheetTargetButton&) = delete;
  ShareSheetTargetButton& operator=(const ShareSheetTargetButton&) = delete;

  // Button is 76px width x 88px height + 8px padding along all sides.
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kButtonWidth, kButtonHeight);
  }
};

SharesheetBubbleView::SharesheetBubbleView(
    views::View* anchor_view,
    sharesheet::SharesheetServiceDelegate* delegate)
    : delegate_(delegate) {
  SetAnchorView(anchor_view);
  CreateBubble();
}

SharesheetBubbleView::SharesheetBubbleView(
    content::WebContents* web_contents,
    sharesheet::SharesheetServiceDelegate* delegate)
    : delegate_(delegate) {
  // TODO(crbug.com/1097623): When supporting open from multiple apps,
  // pass in |app_id| and get NativeWindow from it.
  Profile* const profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  gfx::NativeWindow parent =
      extensions::AppWindowRegistry::Get(profile)
          ->GetCurrentAppWindowForApp(extension_misc::kFilesManagerAppId)
          ->GetNativeWindow();
  set_parent_window(parent);
  parent_view_ = views::Widget::GetWidgetForNativeWindow(parent)->GetRootView();
  UpdateAnchorPosition();

  CreateBubble();
}

SharesheetBubbleView::~SharesheetBubbleView() = default;

void SharesheetBubbleView::ShowBubble(std::vector<TargetInfo> targets,
                                      apps::mojom::IntentPtr intent) {
  targets_ = std::move(targets);
  intent_ = std::move(intent);

  auto* main_layout =
      main_view_->SetLayoutManager(std::make_unique<views::GridLayout>());

  // Set up columnsets
  views::ColumnSet* cs = main_layout->AddColumnSet(COLUMN_SET_ID_TITLE);
  cs->AddColumn(/* h_align */ views::GridLayout::FILL,
                /* v_align */ views::GridLayout::LEADING,
                /* resize_percent */ 0,
                views::GridLayout::ColumnSize::kUsePreferred,
                /* fixed_width */ 0, /*min_width*/ 0);

  // Add Title label
  main_layout->StartRow(views::GridLayout::kFixedSize, COLUMN_SET_ID_TITLE,
                        kTitleLineHeight);
  auto* title = main_layout->AddView(std::make_unique<views::Label>(
      base::UTF8ToUTF16(base::StringPiece(kTitle))));
  title->SetFontList(gfx::FontList("GoogleSans, Medium, 24px"));
  title->SetLineHeight(kTitleLineHeight);
  title->SetEnabledColor(kShareTitleColor);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  main_layout->AddPaddingRow(views::GridLayout::kFixedSize, kSpacing);

  // Add Targets
  auto scrollable_view = std::make_unique<views::View>();
  auto* scroll_layout =
      scrollable_view->SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* cs_buttons =
      scroll_layout->AddColumnSet(COLUMN_SET_ID_TARGETS);
  for (int i = 0; i < kMaxTargetsPerRow; i++) {
    cs_buttons->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                          0, views::GridLayout::ColumnSize::kFixed,
                          kButtonWidth, 0);
  }

  size_t i = 0;
  for (const auto& target : targets_) {
    if (i % kMaxTargetsPerRow == 0) {
      if (i != 0) {
        scroll_layout->AddPaddingRow(views::GridLayout::kFixedSize,
                                     kButtonPadding);
      }
      scroll_layout->StartRow(views::GridLayout::kFixedSize,
                              COLUMN_SET_ID_TARGETS);
    }
    auto target_view = std::make_unique<ShareSheetTargetButton>(
        this, target.display_name, &target.icon);
    target_view->set_tag(i++);
    scroll_layout->AddView(std::move(target_view));
  }

  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetContents(std::move(scrollable_view));
  scroll_view->ClipHeightTo(kTargetViewHeight, kTargetViewHeight);

  main_layout->StartRow(views::GridLayout::kFixedSize, COLUMN_SET_ID_TITLE);
  main_layout->AddView(std::move(scroll_view));
  main_layout->AddPaddingRow(views::GridLayout::kFixedSize, kShortSpacing);

  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);
  GetWidget()->GetRootView()->Layout();
  widget->Show();

  SetToDefaultBubbleSizing();
  UpdateAnchorPosition();
}

void SharesheetBubbleView::ShowActionView() {
  root_view_->SetVisible(false);
  share_action_view_->SetVisible(true);
}

void SharesheetBubbleView::ResizeBubble(const int& width, const int& height) {
  width_ = width;
  height_ = height;
  UpdateAnchorPosition();
}

void SharesheetBubbleView::CloseBubble() {
  views::Widget* widget = View::GetWidget();
  widget->CloseWithReason(views::Widget::ClosedReason::kAcceptButtonClicked);
  // Reset all bubble values.
  targets_.clear();
  active_target_ = base::string16();
  intent_.reset();
  SetToDefaultBubbleSizing();
}

void SharesheetBubbleView::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  auto type = targets_[sender->tag()].type;
  if (type == sharesheet::TargetType::kAction) {
    active_target_ = targets_[sender->tag()].launch_name;
  } else {
    intent_->activity_name = targets_[sender->tag()].activity_name;
  }
  delegate_->OnTargetSelected(targets_[sender->tag()].launch_name, type,
                              std::move(intent_), share_action_view_);
  intent_.reset();
  user_cancelled = false;
}

std::unique_ptr<views::NonClientFrameView>
SharesheetBubbleView::CreateNonClientFrameView(views::Widget* widget) {
  auto bubble_border =
      std::make_unique<views::BubbleBorder>(arrow(), GetShadow(), color());
  bubble_border->SetCornerRadius(kCornerRadius);
  auto frame =
      views::BubbleDialogDelegateView::CreateNonClientFrameView(widget);
  static_cast<views::BubbleFrameView*>(frame.get())
      ->SetBubbleBorder(std::move(bubble_border));
  return frame;
}

void SharesheetBubbleView::OnWidgetDestroyed(views::Widget* widget) {
  // If there is no active_target_ value, the user cancelled without making a
  // selection and we will record this.
  if (user_cancelled) {
    sharesheet::SharesheetMetrics::RecordSharesheetActionMetrics(
        sharesheet::SharesheetMetrics::UserAction::kCancelled);
  }
  delegate_->OnBubbleClosed(active_target_);
}

gfx::Size SharesheetBubbleView::CalculatePreferredSize() const {
  return gfx::Size(width_, height_);
}

void SharesheetBubbleView::CreateBubble() {
  set_close_on_deactivate(true);
  SetButtons(ui::DIALOG_BUTTON_NONE);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  set_margins(gfx::Insets());

  auto root_view = std::make_unique<views::View>();
  root_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(kSpacing), 0,
      true));
  root_view_ = AddChildView(std::move(root_view));

  auto main_view = std::make_unique<views::View>();
  main_view_ = root_view_->AddChildView(std::move(main_view));

  auto share_action_view = std::make_unique<views::View>();
  share_action_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0, true));
  share_action_view_ = AddChildView(std::move(share_action_view));
  share_action_view_->SetVisible(false);
}

void SharesheetBubbleView::UpdateAnchorPosition() {
  // If |width_| is not set, set to default value.
  if (width_ == 0) {
    SetToDefaultBubbleSizing();
  }

  // Horizontally centered
  int x_within_parent_view = parent_view_->GetMirroredXInView(
      (parent_view_->bounds().width() - width_) / 2);
  // Get position in screen, taking parent view origin into account. This is
  // 0,0 in fullscreen on the primary display, but not on secondary displays, or
  // in Hosted App windows.
  gfx::Point origin = parent_view_->GetBoundsInScreen().origin();
  origin += gfx::Vector2d(x_within_parent_view, kBubbleTopPaddingFromWindow);

  // SetAnchorRect will CalculatePreferredSize when called.
  SetAnchorRect(gfx::Rect(origin, gfx::Size()));
}

void SharesheetBubbleView::SetToDefaultBubbleSizing() {
  width_ = kDefaultBubbleWidth;
  height_ = GetHeightForWidth(width_);
}
