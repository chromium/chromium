// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/candidate_window_view.h"

#include <stddef.h>

#include <string>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/input_method/text_field_contextual_info_fetcher.h"
#include "chrome/browser/ui/ash/input_method/candidate_view.h"
#include "chrome/browser/ui/ash/input_method/candidate_window_constants.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/wm/core/window_animations.h"

namespace ui {
namespace ime {

namespace {

class CandidateWindowBorder : public views::BubbleBorder {
 public:
  CandidateWindowBorder()
      : views::BubbleBorder(views::BubbleBorder::TOP_CENTER,
                            views::BubbleBorder::STANDARD_SHADOW) {}
  CandidateWindowBorder(const CandidateWindowBorder&) = delete;
  CandidateWindowBorder& operator=(const CandidateWindowBorder&) = delete;
  ~CandidateWindowBorder() override = default;

  void set_offset(int offset) { offset_ = offset; }

 private:
  // Overridden from views::BubbleBorder:
  gfx::Rect GetBounds(const gfx::Rect& anchor_rect,
                      const gfx::Size& content_size) const override {
    gfx::Rect bounds(content_size);
    bounds.set_origin(gfx::Point(
        anchor_rect.x() - offset_,
        is_arrow_on_top(arrow()) ? anchor_rect.bottom()
                                 : anchor_rect.y() - content_size.height()));

    // It cannot use the normal logic of arrow offset for horizontal offscreen,
    // because the arrow must be in the content's edge. But CandidateWindow has
    // to be visible even when |anchor_rect| is out of the screen.
    gfx::Rect work_area =
        display::Screen::GetScreen()->GetDisplayForNewWindows().work_area();
    if (bounds.right() > work_area.right()) {
      bounds.set_x(work_area.right() - bounds.width());
    }
    if (bounds.x() < work_area.x()) {
      bounds.set_x(work_area.x());
    }

    // For vertical offscreen, we need to check the arrow position first. Only
    // move the candidate window up when the arrow is at the bottom, and only
    // move it down when the arrow is on the top. Otherwise the candidate window
    // will cover the input text which is very bad for user experience. Note
    // that when the arrow is on top and the candidate window is out of the
    // bottom edge of the screen, some other code will change the arrow to
    // bottom to make the candidate window inside screen.
    if (!is_arrow_on_top(arrow()) && bounds.bottom() > work_area.bottom()) {
      bounds.set_y(work_area.bottom() - bounds.height());
    }
    if (is_arrow_on_top(arrow()) && bounds.y() < work_area.y()) {
      bounds.set_y(work_area.y());
    }

    return bounds;
  }

  gfx::Insets GetInsets() const override { return gfx::Insets(); }

  int offset_ = 0;
};

// Computes the page index. For instance, if the page size is 9, and the
// cursor is pointing to 13th candidate, the page index will be 1 (2nd
// page, as the index is zero-origin). Returns -1 on error.
int ComputePageIndex(const ui::CandidateWindow& candidate_window) {
  if (candidate_window.page_size() > 0) {
    return candidate_window.cursor_position() / candidate_window.page_size();
  }
  return -1;
}

// Returns 0-indexed cursor position in a page. See also |ComputePageIndex|.
// Returns -1 on error.
int ComputeIndexInPage(const ui::CandidateWindow& candidate_window) {
  if (candidate_window.page_size() > 0) {
    return candidate_window.cursor_position() % candidate_window.page_size();
  }
  return -1;
}

}  // namespace

class InformationTextArea : public views::View {
  METADATA_HEADER(InformationTextArea, views::View)

 public:
  // InformationTextArea's border is drawn as a separator, it should appear
  // at either top or bottom.
  enum BorderPosition { TOP, BOTTOM };

  // Specify the alignment and initialize the control.
  InformationTextArea(gfx::HorizontalAlignment align, int min_width)
      : min_width_(min_width) {
    label_ = new views::Label;
    label_->SetHorizontalAlignment(align);
    label_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(2, 2, 2, 4)));

    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(label_.get());
  }

  InformationTextArea(const InformationTextArea&) = delete;
  InformationTextArea& operator=(const InformationTextArea&) = delete;

  // views::View:
  void OnThemeChanged() override {
    View::OnThemeChanged();
    SetBackground(views::CreateSolidBackground(color_utils::AlphaBlend(
        SK_ColorBLACK, GetColorProvider()->GetColor(ui::kColorWindowBackground),
        0.0625f)));
    UpdateBorder();
  }

  // Sets the text alignment.
  void SetAlignment(gfx::HorizontalAlignment alignment) {
    label_->SetHorizontalAlignment(alignment);
  }

  // Sets the displayed text.
  void SetText(const std::u16string& text) { label_->SetText(text); }

  // Sets the border thickness for top/bottom.
  void SetBorderFromPosition(BorderPosition position) {
    position_ = position;
    UpdateBorder();
  }

  void UpdateBorder() {
    if (!position_ || !GetWidget()) {
      return;
    }
    SetBorder(views::CreateSolidSidedBorder(
        gfx::Insets::TLBR((position_ == TOP) ? 1 : 0, 0,
                          (position_ == BOTTOM) ? 1 : 0, 0),
        GetColorProvider()->GetColor(ui::kColorMenuBorder)));
  }

 protected:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    gfx::Size size = views::View::CalculatePreferredSize(available_size);
    size.SetToMax(gfx::Size(min_width_, 0));
    return size;
  }

 private:
  raw_ptr<views::Label> label_;
  int min_width_;
  std::optional<BorderPosition> position_;
};

BEGIN_METADATA(InformationTextArea)
END_METADATA

CandidateWindowView::CandidateWindowView(gfx::NativeView parent)
    : selected_candidate_index_in_page_(-1) {
  DialogDelegate::SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetCanActivate(false);
  DCHECK(parent);
  set_parent_window(parent);
  set_margins(gfx::Insets());
  // Ignore this role for accessibility purposes.
  SetAccessibleWindowRole(ax::mojom::Role::kNone);

  // When BubbleDialogDelegateView creates its frame view it will create a
  // bubble border with a non-zero corner radius by default.
  // This class replaces the frame view's bubble border later on with its own
  // |CandidateWindowBorder| with a radius of 0.
  // We want to disable the use of round corners here to ensure that the radius
  // of the frame view created by the BubbleDialogDelegateView is consistent
  // with what CandidateWindowView expects.
  set_use_round_corners(false);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  auxiliary_text_ = new InformationTextArea(gfx::ALIGN_RIGHT, 0);
  preedit_ = new InformationTextArea(gfx::ALIGN_LEFT, kMinPreeditAreaWidth);
  candidate_area_ = new views::View;
  auxiliary_text_->SetVisible(false);
  preedit_->SetVisible(false);
  candidate_area_->SetVisible(false);
  preedit_->SetBorderFromPosition(InformationTextArea::BOTTOM);
  if (candidate_window_.orientation() == ui::CandidateWindow::VERTICAL) {
    AddChildView(preedit_.get());
    AddChildView(candidate_area_.get());
    AddChildView(auxiliary_text_.get());
    auxiliary_text_->SetBorderFromPosition(InformationTextArea::TOP);
    candidate_area_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
  } else {
    AddChildView(preedit_.get());
    AddChildView(auxiliary_text_.get());
    AddChildView(candidate_area_.get());
    auxiliary_text_->SetAlignment(gfx::ALIGN_LEFT);
    auxiliary_text_->SetBorderFromPosition(InformationTextArea::BOTTOM);
    candidate_area_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));
  }
}

CandidateWindowView::~CandidateWindowView() {}

views::Widget* CandidateWindowView::InitWidget() {
  views::Widget* widget = BubbleDialogDelegateView::CreateBubble(this);

  wm::SetWindowVisibilityAnimationTransition(widget->GetNativeView(),
                                             wm::ANIMATE_NONE);

  GetBubbleFrameView()->SetBubbleBorder(
      std::make_unique<CandidateWindowBorder>());
  GetBubbleFrameView()->OnThemeChanged();
  return widget;
}

void CandidateWindowView::OnThemeChanged() {
  BubbleDialogDelegateView::OnThemeChanged();
  SetBorder(views::CreateSolidBorder(
      1, GetColorProvider()->GetColor(ui::kColorMenuBorder)));
}

void CandidateWindowView::UpdateVisibility() {
  if (candidate_area_->GetVisible() || auxiliary_text_->GetVisible() ||
      preedit_->GetVisible()) {
    SizeToContents();
  } else {
    GetWidget()->Close();
  }
}

void CandidateWindowView::HideLookupTable() {
  candidate_area_->SetVisible(false);
  auxiliary_text_->SetVisible(false);
  UpdateVisibility();
}

void CandidateWindowView::HidePreeditText() {
  preedit_->SetVisible(false);
  UpdateVisibility();
}

void CandidateWindowView::ShowPreeditText() {
  preedit_->SetVisible(true);
  UpdateVisibility();
}

void CandidateWindowView::UpdatePreeditText(const std::u16string& text) {
  preedit_->SetText(text);
}

void CandidateWindowView::ShowLookupTable() {
  candidate_area_->SetVisible(true);
  auxiliary_text_->SetVisible(candidate_window_.is_auxiliary_text_visible());
  UpdateVisibility();
}

void CandidateWindowView::UpdateCandidates(
    const ui::CandidateWindow& new_candidate_window) {
  // Updating the candidate views is expensive. We'll skip this if possible.
  if (!candidate_window_.IsEqual(new_candidate_window)) {
    if (candidate_window_.orientation() != new_candidate_window.orientation()) {
      // If the new layout is vertical, the aux text should appear at the
      // bottom. If horizontal, it should appear between preedit and candidates.
      if (new_candidate_window.orientation() == ui::CandidateWindow::VERTICAL) {
        ReorderChildView(auxiliary_text_, children().size());
        auxiliary_text_->SetAlignment(gfx::ALIGN_RIGHT);
        auxiliary_text_->SetBorderFromPosition(InformationTextArea::TOP);
        candidate_area_->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
      } else {
        ReorderChildView(auxiliary_text_, 1);
        auxiliary_text_->SetAlignment(gfx::ALIGN_LEFT);
        auxiliary_text_->SetBorderFromPosition(InformationTextArea::BOTTOM);
        candidate_area_->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));
      }
    }

    // Initialize candidate views if necessary.
    MaybeInitializeCandidateViews(new_candidate_window);

    // Compute the index of the current page.
    const int current_page_index = ComputePageIndex(new_candidate_window);
    if (current_page_index < 0) {
      return;
    }

    // Update the candidates in the current page.
    const size_t start_from =
        current_page_index * new_candidate_window.page_size();

    int max_shortcut_width = 0;
    int max_candidate_width = 0;
    for (size_t i = 0; i < candidate_views_.size(); ++i) {
      const size_t index_in_page = i;
      const size_t candidate_index = start_from + index_in_page;
      CandidateView* candidate_view = candidate_views_[index_in_page];
      // Set the candidate text.
      if (candidate_index < new_candidate_window.candidates().size()) {
        const ui::CandidateWindow::Entry& entry =
            new_candidate_window.candidates()[candidate_index];
        candidate_view->SetEntry(entry);
        candidate_view->SetEnabled(true);
        candidate_view->SetInfolistIcon(!entry.description_title.empty());
      } else {
        // Disable the empty row.
        candidate_view->SetEntry(ui::CandidateWindow::Entry());
        candidate_view->SetEnabled(false);
        candidate_view->SetInfolistIcon(false);
      }
      if (new_candidate_window.orientation() == ui::CandidateWindow::VERTICAL) {
        int shortcut_width = 0;
        int candidate_width = 0;
        candidate_views_[i]->GetPreferredWidths(&shortcut_width,
                                                &candidate_width);
        max_shortcut_width = std::max(max_shortcut_width, shortcut_width);
        max_candidate_width = std::max(max_candidate_width, candidate_width);
      }
    }
    if (new_candidate_window.orientation() == ui::CandidateWindow::VERTICAL) {
      for (ui::ime::CandidateView* view : candidate_views_) {
        view->SetWidths(max_shortcut_width, max_candidate_width);
      }
    }

    std::unique_ptr<CandidateWindowBorder> border =
        std::make_unique<CandidateWindowBorder>();
    if (new_candidate_window.orientation() == ui::CandidateWindow::VERTICAL) {
      border->set_offset(max_shortcut_width);
    } else {
      border->set_offset(0);
    }
    GetBubbleFrameView()->SetBubbleBorder(std::move(border));
    GetBubbleFrameView()->OnThemeChanged();
  }

  const int new_candidate_index_in_page =
      ComputeIndexInPage(new_candidate_window);
  // Notify accessibility if selection changes.
  // Don't notify while showing suggestions, because it interrupts user typing.
  if (new_candidate_window.is_user_selecting()) {
    // Notify when index changes, or when candidate window type changes.
    if (!candidate_window_.is_user_selecting() ||
        (selected_candidate_index_in_page_ != new_candidate_index_in_page &&
         new_candidate_index_in_page != -1)) {
      candidate_views_[new_candidate_index_in_page]->NotifyAccessibilityEvent(
          ax::mojom::Event::kSelection, false);
    }
  }

  // Update the current candidate window. We'll use candidate_window_ from here.
  // Note that SelectCandidateAt() uses candidate_window_.
  candidate_window_.CopyFrom(new_candidate_window);

  // Select the current candidate in the page.
  if (candidate_window_.is_cursor_visible()) {
    if (candidate_window_.page_size()) {
      SelectCandidateAt(new_candidate_index_in_page);
    }
  } else {
    // Unselect the currently selected candidate.
    if (0 <= selected_candidate_index_in_page_ &&
        static_cast<size_t>(selected_candidate_index_in_page_) <
            candidate_views_.size()) {
      candidate_views_[selected_candidate_index_in_page_]->SetHighlighted(
          false);
      selected_candidate_index_in_page_ = -1;
    }
  }

  // Updates auxiliary text
  auxiliary_text_->SetVisible(candidate_window_.is_auxiliary_text_visible());
  auxiliary_text_->SetText(
      base::UTF8ToUTF16(candidate_window_.auxiliary_text()));
}

void CandidateWindowView::SetCursorAndCompositionBounds(
    const gfx::Rect& cursor_bounds,
    const gfx::Rect& composition_bounds) {
  if (base::FeatureList::IsEnabled(ash::features::kImeKoreanModeSwitchDebug)) {
    auto* input_method_manager = ash::input_method::InputMethodManager::Get();

    if (input_method_manager) {
      const std::string& current_input_method_id =
          input_method_manager->GetActiveIMEState()
              ->GetCurrentInputMethod()
              .id();

      if (ash::extension_ime_util::IsCros1pKorean(current_input_method_id)) {
        pending_anchor_rect_ = candidate_window_.show_window_at_composition()
                                   ? composition_bounds
                                   : cursor_bounds;
        ash::input_method::GetTextFieldContextualInfo(base::BindOnce(
            &CandidateWindowView::OnTextFieldContextualInfoAvailable,
            base::Unretained(this)));
        return;
      }
    }
  }

  if (candidate_window_.show_window_at_composition()) {
    SetAnchorRect(composition_bounds);
  } else {
    SetAnchorRect(cursor_bounds);
  }
}

void CandidateWindowView::OnTextFieldContextualInfoAvailable(
    const ash::input_method::TextFieldContextualInfo& info) {
  if (!base::FeatureList::IsEnabled(ash::features::kImeKoreanModeSwitchDebug)) {
    return;
  }

  if (!info.tab_url.DomainIs("docs.google.com")) {
    SetAnchorRect(pending_anchor_rect_);
    return;
  }

  const gfx::Rect& display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  SetAnchorRect(gfx::Rect(80, display_bounds.height() - 60, 0, 0));
}

void CandidateWindowView::MaybeInitializeCandidateViews(
    const ui::CandidateWindow& candidate_window) {
  const ui::CandidateWindow::Orientation orientation =
      candidate_window.orientation();
  const size_t page_size = candidate_window.page_size();

  // Reset all candidate_views_ when orientation changes.
  if (orientation != candidate_window_.orientation()) {
    candidate_area_->RemoveAllChildViews();
    candidate_views_.clear();
  }

  while (page_size < candidate_views_.size()) {
    candidate_area_->RemoveChildViewT(candidate_views_.back());
    candidate_views_.pop_back();
  }

  for (size_t i = candidate_views_.size(); i < page_size; ++i) {
    candidate_views_.push_back(
        candidate_area_->AddChildView(std::make_unique<CandidateView>(
            base::BindRepeating(&CandidateWindowView::CandidateViewPressed,
                                base::Unretained(this), static_cast<int>(i)),
            orientation)));
  }
}

void CandidateWindowView::SelectCandidateAt(int index_in_page) {
  const int current_page_index = ComputePageIndex(candidate_window_);
  if (current_page_index < 0) {
    return;
  }

  const int cursor_absolute_index =
      candidate_window_.page_size() * current_page_index + index_in_page;
  // Ignore click on out of range views.
  if (cursor_absolute_index < 0 ||
      candidate_window_.candidates().size() <=
          static_cast<size_t>(cursor_absolute_index)) {
    return;
  }

  // Remember the currently selected candidate index in the current page.
  selected_candidate_index_in_page_ = index_in_page;

  // Select the candidate specified by index_in_page.
  candidate_views_[index_in_page]->SetHighlighted(true);

  // Update the cursor indexes in the model.
  candidate_window_.set_cursor_position(cursor_absolute_index);
  // Set position data.
  int position_index = candidate_window_.current_candidate_index();
  int total_candidates = candidate_window_.total_candidates();
  if (position_index < 0 || total_candidates < 1 ||
      position_index >= total_candidates) {
    // Sometimes we don't get valid data from |candidate_window_|. In this case,
    // make a best guess about the position and total candidates.
    position_index = index_in_page;
    total_candidates = candidate_window_.candidates().size();
  }
  candidate_views_[index_in_page]->SetPositionData(position_index,
                                                   total_candidates);
}

void CandidateWindowView::CandidateViewPressed(int index) {
  for (Observer& observer : observers_) {
    observer.OnCandidateCommitted(index);
  }
}

BEGIN_METADATA(CandidateWindowView)
END_METADATA

}  // namespace ime
}  // namespace ui
