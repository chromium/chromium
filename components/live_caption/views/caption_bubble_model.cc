// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/views/caption_bubble_model.h"

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "components/live_caption/caption_bubble_context.h"
#include "components/live_caption/views/caption_bubble.h"
#include "media/base/media_switches.h"

namespace {
// The caption bubble contains 2 lines of text in its normal size and 8 lines
// in its expanded size, so the maximum number of lines before truncating is 9.
constexpr int kMaxLines = 9;

// Returns the length of the longest common prefix between two strings.
int GetLongestCommonPrefixLength(const std::string& str1,
                                   const std::string& str2) {
  int length = 0;
  for (unsigned long i = 0, j = 0; i < str1.length() && j < str2.length();
       i++, j++, length++) {
    if (str1[i] != str2[j]) {
      break;
    }
  }

  return length;
}

}  // namespace

namespace captions {

CaptionBubbleModel::CaptionBubbleModel(CaptionBubbleContext* context,
                                       OnCaptionBubbleClosedCallback callback)
    : unique_id_(GetNextId()),
      caption_bubble_closed_callback_(callback),
      context_(context) {
  DCHECK(context_);
}

CaptionBubbleModel::~CaptionBubbleModel() {
  if (base::FeatureList::IsEnabled(media::kLiveCaptionLogFlickerRate)) {
    // Log the number of erasures per partial result.
    double flicker_rate = (partial_result_count_ > 0)
                              ? erasure_count_ / partial_result_count_
                              : 0;
    LOG(WARNING) << "Live caption flicker rate:" << flicker_rate
                 << ". (not a warning)";
  }

  if (observer_)
    observer_->SetModel(nullptr);
}

void CaptionBubbleModel::SetObserver(CaptionBubble* observer) {
  if (observer_)
    return;
  observer_ = observer;
  if (observer_) {
    observer_->OnTextChanged();
    observer_->OnErrorChanged(
        CaptionBubbleErrorType::kGeneric, base::RepeatingClosure(),
        base::BindRepeating(
            [](CaptionBubbleErrorType error_type, bool checked) {}));
  }
}

void CaptionBubbleModel::RemoveObserver() {
  observer_ = nullptr;
}

void CaptionBubbleModel::OnTextChanged() {
  if (observer_)
    observer_->OnTextChanged();
}

void CaptionBubbleModel::OnAutoDetectedLanguageChanged() {
  if (observer_) {
    observer_->OnAutoDetectedLanguageChanged();
  }
}

void CaptionBubbleModel::SetPartialText(const std::string& partial_text) {
  if (base::FeatureList::IsEnabled(media::kLiveCaptionLogFlickerRate)) {
    erasure_count_ +=
        partial_text_.size() -
        GetLongestCommonPrefixLength(partial_text, partial_text_);
    partial_result_count_++;
  }

  partial_text_ = partial_text;
  OnTextChanged();
  if (has_error_) {
    has_error_ = false;
    if (observer_)
      observer_->OnErrorChanged(
          CaptionBubbleErrorType::kGeneric, base::RepeatingClosure(),
          base::BindRepeating(
              [](CaptionBubbleErrorType error_type, bool checked) {}));
  }
}

void CaptionBubbleModel::SetDownloadProgressText(
    const std::u16string& download_progress_text) {
  download_progress_text_ = download_progress_text;

  if (observer_) {
    observer_->OnDownloadProgressTextChanged();
  }
}

void CaptionBubbleModel::OnLanguagePackInstalled() {
  if (observer_) {
    observer_->OnLanguagePackInstalled();
  }
}

void CaptionBubbleModel::CloseButtonPressed() {
  caption_bubble_closed_callback_.Run(context_->GetSessionId());
  Close();
}

void CaptionBubbleModel::Close() {
  is_closed_ = true;
  ClearText();
}

void CaptionBubbleModel::OnError(
    CaptionBubbleErrorType error_type,
    OnErrorClickedCallback error_clicked_callback,
    OnDoNotShowAgainClickedCallback error_silenced_callback) {
  has_error_ = true;
  error_type_ = error_type;
  if (observer_) {
    base::UmaHistogramEnumeration(
        "Accessibility.LiveCaption.CaptionBubbleError", error_type);
    observer_->OnErrorChanged(error_type, std::move(error_clicked_callback),
                              std::move(error_silenced_callback));
  }
}

void CaptionBubbleModel::ClearText() {
  partial_text_.clear();
  final_text_.clear();
  OnTextChanged();
}

void CaptionBubbleModel::CommitPartialText() {
  final_text_ += partial_text_;
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

void CaptionBubbleModel::SetLanguage(const std::string& language_code) {
  if (!observer_) {
    return;
  }

  auto_detected_language_code_ = language_code;
  OnAutoDetectedLanguageChanged();
}

// static
CaptionBubbleModel::Id CaptionBubbleModel::GetNextId() {
  static Id::Generator generator;
  return generator.GenerateNextId();
}

}  // namespace captions
