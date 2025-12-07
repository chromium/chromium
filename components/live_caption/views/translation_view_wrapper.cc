// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/views/translation_view_wrapper.h"

#include "components/live_caption/caption_bubble_settings.h"

namespace captions {

TranslationViewWrapper::TranslationViewWrapper(
    CaptionBubbleSettings* caption_bubble_settings)
    : caption_bubble_settings_(caption_bubble_settings) {}

TranslationViewWrapper::~TranslationViewWrapper() = default;

CaptionBubbleSettings* TranslationViewWrapper::caption_bubble_settings() {
  return caption_bubble_settings_;
}

}  // namespace captions
