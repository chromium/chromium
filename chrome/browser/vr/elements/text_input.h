// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_TEXT_INPUT_H_
#define CHROME_BROWSER_VR_ELEMENTS_TEXT_INPUT_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/model/color_scheme.h"
#include "chrome/browser/vr/model/text_input_info.h"
#include "chrome/browser/vr/text_input_delegate.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace vr {

class Rect;
class Text;

class VR_UI_EXPORT TextInput : public UiElement {
 public:
  // Called when this element receives focus.
  typedef base::RepeatingCallback<void(bool)> OnFocusChangedCallback;
  // Called when the user enters text while this element is focused.
  typedef base::RepeatingCallback<void(const EditedText&)>
      OnInputEditedCallback;
  // Called when the user commits text while this element is focused.
  typedef base::RepeatingCallback<void(const EditedText&)>
      OnInputCommittedCallback;
  TextInput(float font_height_meters,
            OnInputEditedCallback input_edit_callback);
  ~TextInput() override;

  void OnButtonDown(const gfx::PointF& position,
                    base::TimeTicks timestamp) override;
  void OnTouchMove(const gfx::PointF& position,
                   base::TimeTicks timestamp) override;
  void OnButtonUp(const gfx::PointF& position,
                  base::TimeTicks timestamp) override;
  void OnFocusChanged(bool focused) override;
  void OnInputEdited(const EditedText& info) override;
  void OnInputCommitted(const EditedText& info) override;
  void RequestFocus() override;
  void RequestUnfocus() override;
  void UpdateInput(const EditedText& info) override;

  void SetHintText(const base::string16& text);
  void SetTextColor(SkColor color);
  void SetHintColor(SkColor color);
  void SetSelectionColors(const TextSelectionColors& colors);
  void SetTextInputDelegate(TextInputDelegate* text_input_delegate);

  void set_input_committed_callback(const OnInputCommittedCallback& callback) {
    input_commit_callback_ = callback;
  }

  bool OnBeginFrame(const gfx::Transform& head_pose) final;
  void OnSetSize(const gfx::SizeF& size) final;
  void OnSetName() final;

  Text* get_hint_element() { return hint_element_; }
  Text* get_text_element() { return text_element_; }
  Rect* get_cursor_element() { return cursor_element_; }

  EditedText edited_text() const { return edited_text_; }

 private:
  void LayOutNonContributingChildren() final;
  bool SetCursorBlinkState(const base::TimeTicks& time);
  void ResetCursorBlinkCycle();

  virtual void OnUpdateInput(const EditedText& info);

  OnInputEditedCallback input_edit_callback_;
  OnInputEditedCallback input_commit_callback_;
  TextInputDelegate* delegate_ = nullptr;
  EditedText edited_text_;
  bool focused_ = false;
  bool cursor_visible_ = false;
  base::TimeTicks cursor_blink_start_ticks_;

  Text* hint_element_ = nullptr;
  Text* text_element_ = nullptr;
  Rect* cursor_element_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TextInput);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_TEXT_INPUT_H_
