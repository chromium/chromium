// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/text_input.h"

#include <memory>

#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/text.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "ui/gfx/geometry/rect.h"

namespace {
constexpr int kCursorBlinkHalfPeriodMs = 600;
}

namespace vr {

TextInput::TextInput(float font_height_meters,
                     OnInputEditedCallback input_edit_callback)
    : input_edit_callback_(input_edit_callback) {
  auto text = std::make_unique<Text>(font_height_meters);
  text->SetType(kTypeTextInputHint);
  text->SetDrawPhase(kPhaseForeground);
  text->set_focusable(false);
  text->set_contributes_to_parent_bounds(false);
  text->set_x_anchoring(LEFT);
  text->set_x_centering(LEFT);
  text->SetSize(1, 1);
  text->SetLayoutMode(TextLayoutMode::kSingleLineFixedWidth);
  text->SetAlignment(kTextAlignmentLeft);
  hint_element_ = text.get();
  this->AddChild(std::move(text));

  text = std::make_unique<Text>(font_height_meters);
  text->SetType(kTypeTextInputText);
  text->SetDrawPhase(kPhaseForeground);
  text->set_hit_testable(true);
  text->set_focusable(false);
  text->set_contributes_to_parent_bounds(false);
  text->set_x_anchoring(LEFT);
  text->set_x_centering(LEFT);
  text->set_bubble_events(true);
  text->SetSize(1, 1);
  text->SetLayoutMode(TextLayoutMode::kSingleLineFixedWidth);
  text->SetAlignment(kTextAlignmentLeft);
  text->SetCursorEnabled(true);
  text_element_ = text.get();
  this->AddChild(std::move(text));

  auto cursor = std::make_unique<Rect>();
  cursor->SetVisible(false);
  cursor->SetType(kTypeTextInputCursor);
  cursor->SetDrawPhase(kPhaseForeground);
  cursor->set_focusable(false);
  cursor->set_contributes_to_parent_bounds(false);
  cursor->set_x_anchoring(LEFT);
  cursor->set_y_anchoring(BOTTOM);
  cursor->SetColor(SK_ColorBLUE);
  cursor_element_ = cursor.get();
  text_element_->AddChild(std::move(cursor));
}

TextInput::~TextInput() {}

void TextInput::SetTextInputDelegate(TextInputDelegate* text_input_delegate) {
  delegate_ = text_input_delegate;
}

void TextInput::OnButtonDown(const gfx::PointF& position,
                             base::TimeTicks timestamp) {
  // Reposition the cursor based on click position.
  int cursor_position = text_element_->GetCursorPositionFromPoint(position);

  TextInputInfo new_info(edited_text_.current);
  new_info.selection_start = cursor_position;
  new_info.selection_end = cursor_position;
  if (new_info != edited_text_.current) {
    EditedText new_edited_text(edited_text_);
    new_edited_text.Update(new_info);
    UpdateInput(new_edited_text);
  }
}

void TextInput::OnTouchMove(const gfx::PointF& position,
                            base::TimeTicks timestamp) {
  int cursor_position = text_element_->GetCursorPositionFromPoint(position);

  TextInputInfo new_info(edited_text_.current);
  new_info.selection_end = cursor_position;
  if (new_info != edited_text_.current) {
    EditedText new_edited_text(edited_text_);
    new_edited_text.Update(new_info);
    UpdateInput(new_edited_text);
  }
}

void TextInput::OnButtonUp(const gfx::PointF& position,
                           base::TimeTicks timestamp) {
  ResetCursorBlinkCycle();
  RequestFocus();
}

void TextInput::OnFocusChanged(bool focused) {
  focused_ = focused;

  // Update the keyboard with the current text.
  if (delegate_ && focused)
    delegate_->UpdateInput(edited_text_.current);

  if (event_handlers_.focus_change)
    event_handlers_.focus_change.Run(focused);
}

void TextInput::RequestFocus() {
  if (!delegate_ || focused_)
    return;

  delegate_->RequestFocus(id());
}

void TextInput::RequestUnfocus() {
  if (!delegate_ || !focused_)
    return;

  delegate_->RequestUnfocus(id());
}

void TextInput::SetHintText(const base::string16& text) {
  hint_element_->SetText(text);
}

void TextInput::OnInputEdited(const EditedText& info) {
  if (input_edit_callback_)
    input_edit_callback_.Run(info);
}

void TextInput::OnInputCommitted(const EditedText& info) {
  if (input_commit_callback_)
    input_commit_callback_.Run(info);
}

void TextInput::SetTextColor(SkColor color) {
  text_element_->SetColor(color);
}

void TextInput::SetHintColor(SkColor color) {
  hint_element_->SetColor(color);
}

void TextInput::SetSelectionColors(const TextSelectionColors& colors) {
  cursor_element_->SetColor(colors.cursor);
  text_element_->SetSelectionColors(colors);
}

void TextInput::UpdateInput(const EditedText& info) {
  if (edited_text_ == info)
    return;

  OnUpdateInput(info);

  edited_text_ = info;

  if (delegate_ && focused_) {
    delegate_->UpdateInput(info.current);
  }

  text_element_->SetText(info.current.text);
  text_element_->SetSelectionIndices(info.current.selection_start,
                                     info.current.selection_end);
  hint_element_->SetVisible(info.current.text.empty());
}

bool TextInput::OnBeginFrame(const gfx::Transform& head_pose) {
  return SetCursorBlinkState(last_frame_time());
}

void TextInput::OnSetSize(const gfx::SizeF& size) {
  hint_element_->SetFieldWidth(size.width());
  text_element_->SetFieldWidth(size.width());
}

void TextInput::OnSetName() {
  hint_element_->set_owner_name_for_test(name());
  text_element_->set_owner_name_for_test(name());
  cursor_element_->set_owner_name_for_test(name());
}

void TextInput::LayOutNonContributingChildren() {
  // To avoid re-rendering a texture when the cursor blinks, the texture is a
  // separate element. Once the text has been laid out, we can position the
  // cursor appropriately relative to the text field.
  gfx::RectF bounds = text_element_->GetCursorBounds();
  cursor_element_->SetTranslate(bounds.x(), bounds.y(), 0);
  cursor_element_->SetSize(bounds.width(), bounds.height());
}

bool TextInput::SetCursorBlinkState(const base::TimeTicks& time) {
  base::TimeDelta delta = time - cursor_blink_start_ticks_;
  bool visible = focused_ && edited_text_.current.SelectionSize() == 0 &&
                 (delta.InMilliseconds() / kCursorBlinkHalfPeriodMs + 1) % 2;
  if (cursor_visible_ == visible)
    return false;
  cursor_visible_ = visible;
  cursor_element_->SetVisible(visible);
  return true;
}

void TextInput::ResetCursorBlinkCycle() {
  cursor_blink_start_ticks_ = base::TimeTicks::Now();
}

void TextInput::OnUpdateInput(const EditedText& info) {}

}  // namespace vr
