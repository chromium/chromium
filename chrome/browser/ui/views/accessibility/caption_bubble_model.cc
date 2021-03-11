// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/caption_bubble_model.h"

#include "chrome/browser/ui/views/accessibility/caption_bubble.h"

namespace {
// The caption bubble contains 2 lines of text in its normal size and 8 lines
// in its expanded size, so the maximum number of lines before truncating is 9.
constexpr int kMaxLines = 9;
}  // namespace

namespace captions {

CaptionBubbleModel::CaptionBubbleModel() = default;

CaptionBubbleModel::~CaptionBubbleModel() {
  if (observer_)
    observer_->SetModel(nullptr);
}

void CaptionBubbleModel::SetObserver(CaptionBubble* observer) {
  if (observer_)
    return;
  observer_ = observer;
  if (observer_) {
    observer_->OnTextChanged();
    observer_->OnErrorChanged();
  }
}

void CaptionBubbleModel::RemoveObserver() {
  observer_ = nullptr;
}

void CaptionBubbleModel::OnTextChanged() {
  if (observer_)
    observer_->OnTextChanged();
}

void CaptionBubbleModel::SetPartialText(const std::string& partial_text) {
  partial_text_ = partial_text;
  OnTextChanged();
  if (has_error_) {
    has_error_ = false;
    if (observer_)
      observer_->OnErrorChanged();
  }
}

void CaptionBubbleModel::Close() {
  is_closed_ = true;
  ClearText();
}

void CaptionBubbleModel::OnError() {
  has_error_ = true;
  if (observer_)
    observer_->OnErrorChanged();
}

void CaptionBubbleModel::ClearText() {
  partial_text_.clear();
  final_text_.clear();
  OnTextChanged();
}

void CaptionBubbleModel::CommitPartialText() {
  final_text_ += partial_text_;

  // If the first character of partial text isn't a space, add a space before
  // appending it to final text. There is no need to alert the observer because
  // the text itself has not changed, just its representation, and there is no
  // need to render a trailing space.
  // TODO(crbug.com/1055150): This feature is launching for English first.
  // Make sure spacing is correct for all languages.
  if (partial_text_.size() > 0 &&
      partial_text_.compare(partial_text_.size() - 1, 1, " ") != 0) {
    final_text_ += " ";
  }
  partial_text_.clear();

  if (!observer_)
    return;

  // Truncate the final text to kMaxLines lines long. This time, alert the
  // observer that the text has changed.
  const size_t num_lines = observer_->GetNumLinesInLabel();
  if (num_lines > kMaxLines) {
    const size_t truncate_index =
        observer_->GetTextIndexOfLineInLabel(num_lines - kMaxLines);
    final_text_.erase(0, truncate_index);
    OnTextChanged();
  }
}

}  // namespace captions
