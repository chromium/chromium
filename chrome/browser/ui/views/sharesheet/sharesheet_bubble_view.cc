// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharesheet/sharesheet_bubble_view.h"

#include <memory>
#include <utility>

#include "base/strings/string16.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_metrics.h"
#include "chrome/browser/sharesheet/sharesheet_service_delegate.h"
#include "chrome/browser/ui/views/sharesheet/sharesheet_expand_button.h"
#include "chrome/browser/ui/views/sharesheet/sharesheet_target_button.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
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
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace {

// TODO(crbug.com/1097623) Many of below values are sums of each other and
// can be removed.

// Sizes are in px.
constexpr int kButtonPadding = 8;
constexpr int kButtonWidth = 92;
constexpr int kCornerRadius = 12;
constexpr int kBubbleTopPaddingFromWindow = 36;
constexpr int kDefaultBubbleWidth = 416;
constexpr int kNoExtensionBubbleHeight = 340;
constexpr int kDefaultBubbleHeight = 380;
constexpr int kExpandedBubbleHeight = 522;
constexpr int kMaxTargetsPerRow = 4;
constexpr int kMaxRowsForDefaultView = 2;

// TargetViewHeight is 2*kButtonHeight + kButtonPadding
constexpr int kTargetViewHeight = 216;
constexpr int kTargetViewExpandedHeight = 382;

constexpr int kExpandViewTitleLabelHeight = 22;
constexpr int kExpandViewPadding = 16;

constexpr int kShortSpacing = 20;
constexpr int kSpacing = 24;
constexpr int kTitleLineHeight = 24;

constexpr char kTitleFont[] = "GoogleSans, Medium, 16px";
constexpr char kExpandViewTitleFont[] = "Roboto, Medium, 15px";

constexpr SkColor kShareTitleColor = gfx::kGoogleGrey900;
constexpr SkColor kShareTargetTitleColor = gfx::kGoogleGrey700;

enum { COLUMN_SET_ID_TITLE, COLUMN_SET_ID_TARGETS };

void SetUpTargetColumnSet(views::GridLayout* layout) {
  views::ColumnSet* cs = layout->AddColumnSet(COLUMN_SET_ID_TARGETS);
  for (int i = 0; i < kMaxTargetsPerRow; i++) {
    cs->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER, 0,
                  views::GridLayout::ColumnSize::kFixed, kButtonWidth, 0);
  }
}

}  // namespace

SharesheetBubbleView::SharesheetBubbleView(
    content::WebContents* web_contents,
    sharesheet::SharesheetServiceDelegate* delegate)
    : delegate_(delegate) {
  gfx::NativeWindow parent = web_contents->GetTopLevelNativeWindow();
  set_parent_window(parent);
  parent_view_ = views::Widget::GetWidgetForNativeWindow(parent)->GetRootView();
  UpdateAnchorPosition();

  CreateBubble();
}

SharesheetBubbleView::~SharesheetBubbleView() = default;

void SharesheetBubbleView::ShowBubble(
    std::vector<TargetInfo> targets,
    apps::mojom::IntentPtr intent,
    sharesheet::CloseCallback close_callback) {
  targets_ = std::move(targets);
  intent_ = std::move(intent);
  close_callback_ = std::move(close_callback);

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
      l10n_util::GetStringUTF16(IDS_SHARESHEET_TITLE_LABEL)));
  title->SetFontList(gfx::FontList(kTitleFont));
  title->SetLineHeight(kTitleLineHeight);
  title->SetEnabledColor(kShareTitleColor);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  main_layout->AddPaddingRow(views::GridLayout::kFixedSize, kSpacing);

  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetContents(MakeScrollableTargetView());
  scroll_view->ClipHeightTo(kTargetViewHeight, kTargetViewExpandedHeight);

  // TODO(crbug.com/1097623) Update grey border lines.
  main_layout->StartRow(views::GridLayout::kFixedSize, COLUMN_SET_ID_TITLE);
  main_layout->AddView(std::move(scroll_view));
  main_layout->AddPaddingRow(views::GridLayout::kFixedSize, kShortSpacing);

  main_layout->StartRow(views::GridLayout::kFixedSize, COLUMN_SET_ID_TITLE,
                        kShortSpacing);
  expand_button_ =
      main_layout->AddView(std::make_unique<SharesheetExpandButton>(this));
  main_layout->AddPaddingRow(views::GridLayout::kFixedSize, kShortSpacing);

  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);
  GetWidget()->GetRootView()->Layout();
  widget->Show();

  if (targets_.size() <= (kMaxRowsForDefaultView * kMaxTargetsPerRow)) {
    width_ = kDefaultBubbleWidth;
    height_ = kNoExtensionBubbleHeight;
  } else {
    SetToDefaultBubbleSizing();
  }
  UpdateAnchorPosition();
}

std::unique_ptr<views::View> SharesheetBubbleView::MakeScrollableTargetView() {
  // Set up default and expanded views.
  auto default_view = std::make_unique<views::View>();
  auto* default_layout =
      default_view->SetLayoutManager(std::make_unique<views::GridLayout>());
  SetUpTargetColumnSet(default_layout);

  auto expanded_view = std::make_unique<views::View>();
  auto* expanded_layout =
      expanded_view->SetLayoutManager(std::make_unique<views::GridLayout>());
  SetUpTargetColumnSet(expanded_layout);
  views::ColumnSet* cs_expanded_view =
      expanded_layout->AddColumnSet(COLUMN_SET_ID_TITLE);
  cs_expanded_view->AddColumn(/* h_align */ views::GridLayout::FILL,
                              /* v_align */ views::GridLayout::CENTER,
                              /* resize_percent */ 1.0,
                              views::GridLayout::ColumnSize::kUsePreferred,
                              /* fixed_width */ 0, /*min_width*/ 0);

  // Add Extended View Title
  expanded_layout->AddPaddingRow(views::GridLayout::kFixedSize,
                                 kExpandViewPadding);
  expanded_layout->StartRow(views::GridLayout::kFixedSize, COLUMN_SET_ID_TITLE,
                            kExpandViewTitleLabelHeight);
  auto* app_list_label =
      expanded_layout->AddView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_SHARESHEET_APPS_LIST_LABEL)));
  app_list_label->SetFontList(gfx::FontList(kExpandViewTitleFont));
  app_list_label->SetLineHeight(kExpandViewTitleLabelHeight);
  app_list_label->SetEnabledColor(kShareTargetTitleColor);
  app_list_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  expanded_layout->AddPaddingRow(views::GridLayout::kFixedSize,
                                 kExpandViewPadding);

  PopulateLayoutsWithTargets(default_layout, expanded_layout);
  default_layout->AddPaddingRow(views::GridLayout::kFixedSize, kShortSpacing);

  auto scrollable_view = std::make_unique<views::View>();
  auto* layout =
      scrollable_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  scrollable_view->AddChildView(std::move(default_view));
  expanded_view_ = scrollable_view->AddChildView(std::move(expanded_view));

  // Expanded view is not visible by default.
  expanded_view_->SetVisible(false);

  return scrollable_view;
}

void SharesheetBubbleView::PopulateLayoutsWithTargets(
    views::GridLayout* default_layout,
    views::GridLayout* expanded_layout) {
  // Add first kMaxRowsForDefaultView*kMaxTargetsPerRow targets to
  // |default_view| and subsequent targets to |expanded_view|.
  size_t target_counter = 0;
  size_t row_count = 0;
  auto* layout_for_target = default_layout;
  for (const auto& target : targets_) {
    if (target_counter % kMaxTargetsPerRow == 0) {
      // When we've reached kMaxRowsForDefaultView switch to populating
      // |expanded_layout|.
      if (row_count == kMaxRowsForDefaultView) {
        layout_for_target = expanded_layout;
        // Do not add a padding row if we are at the first row of
        // |default_layout| or |expanded_layout|.
      } else if (row_count != 0) {
        layout_for_target->AddPaddingRow(views::GridLayout::kFixedSize,
                                         kButtonPadding);
      }
      ++row_count;
      layout_for_target->StartRow(views::GridLayout::kFixedSize,
                                  COLUMN_SET_ID_TARGETS);
    }

    base::string16 secondary_display_name =
        target.secondary_display_name.value_or(base::string16());

    auto target_view = std::make_unique<SharesheetTargetButton>(
        this, target.display_name, secondary_display_name, &target.icon);
    target_view->set_tag(target_counter++);

    layout_for_target->AddView(std::move(target_view));
  }
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
  if (sender == expand_button_) {
    if (show_expanded_view_) {
      expand_button_->SetDefaultView();
      expanded_view_->SetVisible(false);
      ResizeBubble(kDefaultBubbleWidth, kDefaultBubbleHeight);
    } else {
      expand_button_->SetExpandedView();
      expanded_view_->SetVisible(true);
      ResizeBubble(kDefaultBubbleWidth, kExpandedBubbleHeight);
    }
    show_expanded_view_ = !show_expanded_view_;
  } else {
    auto type = targets_[sender->tag()].type;
    if (type == sharesheet::TargetType::kAction) {
      active_target_ = targets_[sender->tag()].launch_name;
    } else {
      intent_->activity_name = targets_[sender->tag()].activity_name;
    }
    delegate_->OnTargetSelected(targets_[sender->tag()].launch_name, type,
                                std::move(intent_), share_action_view_);
    intent_.reset();
    user_cancelled_ = false;
    if (close_callback_) {
      std::move(close_callback_).Run(sharesheet::SharesheetResult::kSuccess);
    }
  }
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
  if (user_cancelled_) {
    sharesheet::SharesheetMetrics::RecordSharesheetActionMetrics(
        sharesheet::SharesheetMetrics::UserAction::kCancelled);
  }
  delegate_->OnBubbleClosed(active_target_);
  if (close_callback_) {
    std::move(close_callback_).Run(sharesheet::SharesheetResult::kCancel);
  }
}

gfx::Size SharesheetBubbleView::CalculatePreferredSize() const {
  return gfx::Size(width_, height_);
}

void SharesheetBubbleView::CreateBubble() {
  set_close_on_deactivate(true);
  SetButtons(ui::DIALOG_BUTTON_NONE);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // Margins must be set to 0 or share_action_view will have undesired margins.
  set_margins(gfx::Insets());

  // TODO(crbug.com/1097623) Remove root_view which is only used to set up
  // margins for |main_view_|.
  auto root_view = std::make_unique<views::View>();
  root_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(/* top */ kSpacing, /* left */ kSpacing,
                  /* bottom */ kShortSpacing, /* right */ kSpacing),
      0, true));
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
  height_ = kDefaultBubbleHeight;
}
