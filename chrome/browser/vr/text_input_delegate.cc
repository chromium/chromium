// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/text_input_delegate.h"
#include "chrome/browser/vr/model/text_input_info.h"

namespace vr {

TextInputDelegate::TextInputDelegate() {}

TextInputDelegate::~TextInputDelegate() = default;

void TextInputDelegate::SetRequestFocusCallback(
    const RequestFocusCallback& callback) {
  request_focus_callback_ = callback;
}

void TextInputDelegate::SetRequestUnfocusCallback(
    const RequestUnfocusCallback& callback) {
  request_unfocus_callback_ = callback;
}

void TextInputDelegate::SetUpdateInputCallback(
    const UpdateInputCallback& callback) {
  update_input_callback_ = callback;
}

void TextInputDelegate::RequestFocus(int element_id) {
  if (!request_focus_callback_.is_null())
    request_focus_callback_.Run(element_id);
}

void TextInputDelegate::RequestUnfocus(int element_id) {
  if (!request_unfocus_callback_.is_null())
    request_unfocus_callback_.Run(element_id);
}

void TextInputDelegate::UpdateInput(const vr::TextInputInfo& info) {
  if (!update_input_callback_.is_null())
    update_input_callback_.Run(info);
}

}  // namespace vr
