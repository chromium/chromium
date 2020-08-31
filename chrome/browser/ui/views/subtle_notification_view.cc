// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/subtle_notification_view.h"

#include <memory>

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

namespace {

// Space between the site info label.
const int kMiddlePaddingPx = 30;

const int kOuterPaddingHorizPx = 40;
const int kOuterPaddingVertPx = 8;

// Partially-transparent background color.
const SkColor kSubtleNotificationBackgroundColor =
    SkColorSetARGB(0xcc, 0x28, 0x2c, 0x32);

// Spacing around the key name.
const int kKeyNameMarginHorizPx = 7;
const int kKeyNameBorderPx = 1;
const int kKeyNameCornerRadius = 2;
const int kKeyNamePaddingPx = 5;

// The context used to obtain typography for the instruction text. It's not
// really a dialog, but a dialog title is a good fit.
constexpr int kInstructionTextContext = views::style::CONTEXT_DIALOG_TITLE;

// Delimiter indicating there should be a segment displayed as a keyboard key.
const char kKeyNameDelimiter[] = "|";

}  // namespace

// Class containing the instruction text. Contains fancy styling on the keyboard
// key (not just a simple label).
class SubtleNotificationView::InstructionView : public views::View {
 public:
  // Creates an InstructionView with specific text. |text| may contain one or
  // more segments delimited by a pair of pipes ('|'); each of these segments
  // will be displayed as a keyboard key. e.g., "Press |Alt|+|Q| to exit" will
  // have "Alt" and "Q" rendered as keys.
  explicit InstructionView(const base::string16& text);

  const base::string16 text() const { return text_; }
  void SetText(const base::string16& text);

 private:
  // Adds a label to the end of the notification text. If |format_as_key|,
  // surrounds the label in a rounded-rect border to indicate that it is a
  // keyboard key.
  void AddTextSegment(const base::string16& text, bool format_as_key);

  base::string16 text_;

  DISALLOW_COPY_AND_ASSIGN(InstructionView);
};

SubtleNotificationView::InstructionView::InstructionView(
    const base::string16& text) {
  // The |between_child_spacing| is the horizontal margin of the key name.
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kKeyNameMarginHorizPx));

  SetText(text);
}

void SubtleNotificationView::InstructionView::SetText(
    const base::string16& text) {
  // Avoid replacing the contents with the same text.
  if (text == text_)
    return;

  RemoveAllChildViews(true);

  // Parse |text|, looking for pipe-delimited segment.
  std::vector<base::string16> segments =
      base::SplitString(text, base::ASCIIToUTF16(kKeyNameDelimiter),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  // SplitString() returns empty strings for zero-length segments, so given an
  // even number of pipes, there should always be an odd number of segments.
  // The exception is if |text| is entirely empty, in which case the returned
  // list is also empty (rather than containing a single empty string).
  DCHECK(segments.empty() || segments.size() % 2 == 1);

  // Add text segment, alternating between non-key (no border) and key (border)
  // formatting.
  bool format_as_key = false;
  for (const auto& segment : segments) {
    AddTextSegment(segment, format_as_key);
    format_as_key = !format_as_key;
  }

  text_ = text;
}

void SubtleNotificationView::InstructionView::AddTextSegment(
    const base::string16& text, bool format_as_key) {
  constexpr SkColor kForegroundColor = SK_ColorWHITE;

  views::Label* label = new views::Label(text, kInstructionTextContext);
  label->SetEnabledColor(kForegroundColor);
  label->SetBackgroundColor(kSubtleNotificationBackgroundColor);

  if (!format_as_key) {
    AddChildView(label);
    return;
  }

  views::View* key = new views::View;
  auto key_name_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(0, kKeyNamePaddingPx), 0);
  key_name_layout->set_minimum_cross_axis_size(
      label->GetPreferredSize().height() + kKeyNamePaddingPx * 2);
  key->SetLayoutManager(std::move(key_name_layout));
  key->AddChildView(label);
  // The key name has a border around it.
  std::unique_ptr<views::Border> border(views::CreateRoundedRectBorder(
      kKeyNameBorderPx, kKeyNameCornerRadius, kForegroundColor));
  key->SetBorder(std::move(border));
  AddChildView(key);
}

SubtleNotificationView::SubtleNotificationView() : instruction_view_(nullptr) {
  std::unique_ptr<views::BubbleBorder> bubble_border(new views::BubbleBorder(
      views::BubbleBorder::NONE, views::BubbleBorder::NO_ASSETS,
      kSubtleNotificationBackgroundColor));
  SetBackground(std::make_unique<views::BubbleBackground>(bubble_border.get()));
  SetBorder(std::move(bubble_border));

  instruction_view_ = new InstructionView(base::string16());

  int outer_padding_horiz = kOuterPaddingHorizPx;
  int outer_padding_vert = kOuterPaddingVertPx;
  AddChildView(instruction_view_);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(outer_padding_vert, outer_padding_horiz), kMiddlePaddingPx));
}

SubtleNotificationView::~SubtleNotificationView() {}

void SubtleNotificationView::UpdateContent(
    const base::string16& instruction_text) {
  instruction_view_->SetText(instruction_text);
  instruction_view_->SetVisible(!instruction_text.empty());
  Layout();
}

// static
views::Widget* SubtleNotificationView::CreatePopupWidget(
    gfx::NativeView parent_view,
    std::unique_ptr<SubtleNotificationView> view) {
  // Initialize the popup.
  views::Widget* popup = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = parent_view;
  params.accept_events = false;
  popup->Init(std::move(params));
  popup->SetContentsView(std::move(view));
  // We set layout manager to nullptr to prevent the widget from sizing its
  // contents to the same size as itself. This prevents the widget contents from
  // shrinking while we animate the height of the popup to give the impression
  // that it is sliding off the top of the screen.
  // TODO(mgiuca): This probably isn't necessary now that there is no slide
  // animation. Remove it.
  popup->GetRootView()->SetLayoutManager(nullptr);

  return popup;
}

void SubtleNotificationView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kAlert;
  base::string16 accessible_name;
  base::RemoveChars(instruction_view_->text(),
                    base::ASCIIToUTF16(kKeyNameDelimiter), &accessible_name);
  node_data->SetName(accessible_name);
}
