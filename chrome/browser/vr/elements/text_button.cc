// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/text_button.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/vr/elements/rect.h"

namespace vr {

namespace {

constexpr float kTextPaddingRatio = 0.7f;

}  // namespace

TextButton::TextButton(float text_size, AudioDelegate* audio_delegate)
    : Button(base::DoNothing(), audio_delegate) {
  set_hover_offset(0.0f);
  set_bounds_contain_children(true);

  // Add the text object to the button.
  auto text = std::make_unique<Text>(text_size);
  text->SetDrawPhase(kPhaseForeground);
  text->SetType(kTypeButtonText);
  text->SetLayoutMode(kSingleLine);
  text->set_hit_testable(false);
  text_ = text.get();
  background()->AddChild(std::move(text));

  // Configure background to size with text.
  background()->set_bounds_contain_children(true);
  background()->set_contributes_to_parent_bounds(true);
  background()->set_padding(text_size * kTextPaddingRatio,
                            text_size * kTextPaddingRatio);
}

TextButton::~TextButton() = default;

void TextButton::SetText(const std::u16string& text) {
  text_->SetText(text);
}

void TextButton::OnSetColors(const ButtonColors& colors) {
  if (!enabled()) {
    text_->SetColor(colors.foreground_disabled);
  } else {
    text_->SetColor(colors.foreground);
  }
}

}  // namespace vr
