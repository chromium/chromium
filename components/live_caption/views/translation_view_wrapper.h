// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_VIEWS_TRANSLATION_VIEW_WRAPPER_H_
#define COMPONENTS_LIVE_CAPTION_VIEWS_TRANSLATION_VIEW_WRAPPER_H_

#include "base/memory/raw_ptr.h"
#include "components/live_caption/views/translation_view_wrapper_base.h"

namespace captions {

class CaptionBubbleSettings;

class TranslationViewWrapper : public TranslationViewWrapperBase {
 public:
  explicit TranslationViewWrapper(
      CaptionBubbleSettings* caption_bubble_settings);

  TranslationViewWrapper(const TranslationViewWrapper&) = delete;
  TranslationViewWrapper& operator=(const TranslationViewWrapper&) = delete;

  ~TranslationViewWrapper() override;

 protected:
  CaptionBubbleSettings* caption_bubble_settings() override;

 private:
  raw_ptr<CaptionBubbleSettings> caption_bubble_settings_;
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_VIEWS_TRANSLATION_VIEW_WRAPPER_H_
