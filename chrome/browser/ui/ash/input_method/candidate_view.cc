// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/candidate_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/ash/input_method/candidate_window_constants.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/candidate_window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/widget/widget.h"

namespace ui::ime {

namespace {

// VerticalCandidateLabel is used for rendering candidate text in
// the vertical candidate window.
class VerticalCandidateLabel : public views::Label {
  METADATA_HEADER(VerticalCandidateLabel, views::Label)

 public:
  VerticalCandidateLabel() = default;
  VerticalCandidateLabel(const VerticalCandidateLabel&) = delete;
  VerticalCandidateLabel& operator=(const VerticalCandidateLabel&) = delete;
  ~VerticalCandidateLabel() override = default;

 private:
  // views::Label:
  // Returns the preferred size, but guarantees that the width has at
  // least kMinCandidateLabelWidth pixels.
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    gfx::Size size = Label::CalculatePreferredSize(available_size);
    size.SetToMax(gfx::Size(kMinCandidateLabelWidth, 0));
    size.SetToMin(gfx::Size(kMaxCandidateLabelWidth, size.height()));
    return size;
  }
};

BEGIN_METADATA(VerticalCandidateLabel)
END_METADATA

// The label text is not set in this class.
class ShortcutLabel : public views::Label {
  METADATA_HEADER(ShortcutLabel, views::Label)

 public:
  explicit ShortcutLabel(ui::CandidateWindow::Orientation orientation)
      : orientation_(orientation) {
    // TODO(tapted): Get this FontList from views::style.
    if (orientation == ui::CandidateWindow::VERTICAL) {
      SetFontList(font_list().Derive(kFontSizeDelta, gfx::Font::NORMAL,
                                     gfx::Font::Weight::BOLD));
    } else {
      SetFontList(font_list().DeriveWithSizeDelta(kFontSizeDelta));
    }
    // TODO(satorux): Maybe we need to use language specific fonts for
    // candidate_label, like Chinese font for Chinese input method?

    // Setup paddings.
    const auto kVerticalShortcutLabelInsets = gfx::Insets::TLBR(1, 6, 1, 6);
    const auto kHorizontalShortcutLabelInsets = gfx::Insets::TLBR(1, 3, 1, 0);
    const gfx::Insets insets = (orientation == ui::CandidateWindow::VERTICAL
                                    ? kVerticalShortcutLabelInsets
                                    : kHorizontalShortcutLabelInsets);
    SetBorder(views::CreateEmptyBorder(insets));

    SetElideBehavior(gfx::NO_ELIDE);
  }
  ShortcutLabel(const ShortcutLabel&) = delete;
  ShortcutLabel& operator=(const ShortcutLabel&) = delete;
  ~ShortcutLabel() override = default;

  // views::Label:
  void OnThemeChanged() override {
    Label::OnThemeChanged();
    // Add decoration based on the orientation.
    if (orientation_ == ui::CandidateWindow::VERTICAL) {
      // Set the background color.
      SkColor blackish = color_utils::AlphaBlend(
          SK_ColorBLACK,
          GetColorProvider()->GetColor(ui::kColorWindowBackground), 0.25f);
      SetBackground(views::CreateSolidBackground(SkColorSetA(blackish, 0xE0)));
    }
  }

 private:
  const ui::CandidateWindow::Orientation orientation_;
};

BEGIN_METADATA(ShortcutLabel)
END_METADATA

// Creates an annotation label. Sets no text by default.
std::unique_ptr<views::Label> CreateAnnotationLabel() {
  auto label = views::Builder<views::Label>()
                   .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                   .SetElideBehavior(gfx::NO_ELIDE)
                   .SetEnabledColorId(ui::kColorLabelForegroundSecondary)
                   .Build();
  label->SetFontList(label->font_list().DeriveWithSizeDelta(kFontSizeDelta));
  return label;
}

// Creates the candidate label, and returns it (never returns nullptr).
// The label text is not set in this function.
std::unique_ptr<views::Label> CreateCandidateLabel(
    ui::CandidateWindow::Orientation orientation) {
  std::unique_ptr<views::Label> candidate_label;

  if (orientation == ui::CandidateWindow::VERTICAL) {
    candidate_label = std::make_unique<VerticalCandidateLabel>();
  } else {
    candidate_label = std::make_unique<views::Label>();
  }

  // Change the font size.
  candidate_label->SetFontList(
      candidate_label->font_list().DeriveWithSizeDelta(kFontSizeDelta));
  candidate_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  candidate_label->SetElideBehavior(gfx::NO_ELIDE);

  return candidate_label;
}

}  // namespace

CandidateView::CandidateView(PressedCallback callback,
                             ui::CandidateWindow::Orientation orientation)
    : views::Button(std::move(callback)), orientation_(orientation) {
  SetBorder(views::CreateEmptyBorder(1));

  shortcut_label_ = AddChildView(std::make_unique<ShortcutLabel>(orientation));
  candidate_label_ = AddChildView(CreateCandidateLabel(orientation));
  annotation_label_ = AddChildView(CreateAnnotationLabel());

  if (orientation == ui::CandidateWindow::VERTICAL) {
    infolist_icon_ =
        AddChildView(views::Builder<views::View>()
                         .SetBackground(views::CreateThemedSolidBackground(
                             ui::kColorFocusableBorderFocused))
                         .Build());
  }

  SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  GetViewAccessibility().SetRole(ax::mojom::Role::kImeCandidate);
}

void CandidateView::GetPreferredWidths(int* shortcut_width,
                                       int* candidate_width) {
  *shortcut_width =
      shortcut_label_
          ->GetPreferredSize(views::SizeBounds(shortcut_label_->width(), {}))
          .width();
  *candidate_width =
      candidate_label_
          ->GetPreferredSize(views::SizeBounds(candidate_label_->width(), {}))
          .width();
}

void CandidateView::SetWidths(int shortcut_width, int candidate_width) {
  shortcut_width_ = shortcut_width;
  shortcut_label_->SetVisible(shortcut_width_ != 0);
  candidate_width_ = candidate_width;
}

void CandidateView::SetEntry(const ui::CandidateWindow::Entry& entry) {
  std::u16string label = entry.label;
  if (!label.empty() && orientation_ != ui::CandidateWindow::VERTICAL) {
    label += u".";
  }
  shortcut_label_->SetText(label);
  candidate_label_->SetText(entry.value);
  annotation_label_->SetText(entry.annotation);
  GetViewAccessibility().SetName(entry.value);
}

void CandidateView::SetInfolistIcon(bool enable) {
  if (infolist_icon_) {
    infolist_icon_->SetVisible(enable);
  }
  SchedulePaint();
}

void CandidateView::SetHighlighted(bool highlighted) {
  if (highlighted_ == highlighted) {
    return;
  }

  highlighted_ = highlighted;
  if (highlighted) {
    SetBackground(views::CreateThemedSolidBackground(
        ui::kColorTextfieldSelectionBackground));
    SetBorder(
        views::CreateThemedSolidBorder(1, ui::kColorFocusableBorderFocused));

    // Cancel currently focused one.
    for (View* view : parent()->children()) {
      if (view != this) {
        static_cast<CandidateView*>(view)->SetHighlighted(false);
      }
    }
  } else {
    SetBackground(nullptr);
    SetBorder(views::CreateEmptyBorder(1));
  }
  SchedulePaint();
}

void CandidateView::StateChanged(ButtonState old_state) {
  Button::StateChanged(old_state);
  int text_style = GetState() == STATE_DISABLED ? views::style::STYLE_DISABLED
                                                : views::style::STYLE_PRIMARY;
  shortcut_label_->SetEnabledColorId(
      views::TypographyProvider::Get().GetColorId(views::style::CONTEXT_LABEL,
                                                  text_style));
  if (GetState() == STATE_PRESSED) {
    SetHighlighted(true);
  }
}

bool CandidateView::OnMouseDragged(const ui::MouseEvent& event) {
  if (!HitTestPoint(event.location())) {
    // Moves the drag target to the sibling view.
    gfx::Point location_in_widget(event.location());
    ConvertPointToWidget(this, &location_in_widget);
    for (View* view : parent()->children()) {
      if (view == this) {
        continue;
      }
      gfx::Point location_in_sibling(location_in_widget);
      ConvertPointFromWidget(view, &location_in_sibling);
      if (view->HitTestPoint(location_in_sibling)) {
        GetWidget()->GetRootView()->SetMouseAndGestureHandler(view);
        auto* sibling = static_cast<CandidateView*>(view);
        sibling->SetHighlighted(true);
        return view->OnMouseDragged(ui::MouseEvent(event, this, sibling));
      }
    }

    return false;
  }

  return views::Button::OnMouseDragged(event);
}

void CandidateView::Layout(PassKey) {
  const int padding_width =
      orientation_ == ui::CandidateWindow::VERTICAL ? 4 : 6;
  int x = 0;
  shortcut_label_->SetBounds(x, 0, shortcut_width_, height());
  if (shortcut_width_ > 0) {
    x += shortcut_width_ + padding_width;
  }
  candidate_label_->SetBounds(x, 0, candidate_width_, height());
  x += candidate_width_ + padding_width;

  int right = bounds().right();
  if (infolist_icon_ && infolist_icon_->GetVisible()) {
    infolist_icon_->SetBounds(
        right - kInfolistIndicatorIconWidth - kInfolistIndicatorIconPadding,
        kInfolistIndicatorIconPadding, kInfolistIndicatorIconWidth,
        height() - kInfolistIndicatorIconPadding * 2);
    right -= kInfolistIndicatorIconWidth + kInfolistIndicatorIconPadding * 2;
  }
  annotation_label_->SetBounds(x, 0, right - x, height());
}

gfx::Size CandidateView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int padding_width =
      orientation_ == ui::CandidateWindow::VERTICAL ? 4 : 6;
  gfx::Size size;
  if (shortcut_label_->GetVisible()) {
    size = shortcut_label_->GetPreferredSize(
        views::SizeBounds(shortcut_width_, {}));
    size.SetToMax(gfx::Size(shortcut_width_, 0));
    size.Enlarge(padding_width, 0);
  }
  gfx::Size candidate_size = candidate_label_->GetPreferredSize(
      views::SizeBounds(candidate_width_, {}));
  candidate_size.SetToMax(gfx::Size(candidate_width_, 0));
  size.Enlarge(candidate_size.width() + padding_width, 0);
  size.SetToMax(candidate_size);
  int reserve_margin =
      kInfolistIndicatorIconWidth + kInfolistIndicatorIconPadding * 2;
  if (annotation_label_->GetVisible()) {
    views::SizeBound available_width = std::max<views::SizeBound>(
        0, available_size.width() - size.width() - reserve_margin);
    gfx::Size annotation_size = annotation_label_->GetPreferredSize(
        views::SizeBounds(available_width, {}));
    size.Enlarge(annotation_size.width() + padding_width, 0);
    size.SetToMax(annotation_size);
  }

  // Reserves the margin for infolist_icon even if it's not visible.
  size.Enlarge(reserve_margin, 0);
  return size;
}

void CandidateView::SetPositionData(int index, int total) {
  candidate_index_ = index;
  total_candidates_ = total;

  // PosInSet needs to be incremented since |candidate_index_| is 0-based.
  GetViewAccessibility().SetPosInSet(candidate_index_ + 1);
  GetViewAccessibility().SetSetSize(total_candidates_);
}

BEGIN_METADATA(CandidateView)
END_METADATA

}  // namespace ui::ime
