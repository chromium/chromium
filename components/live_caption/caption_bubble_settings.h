// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_CAPTION_BUBBLE_SETTINGS_H_
#define COMPONENTS_LIVE_CAPTION_CAPTION_BUBBLE_SETTINGS_H_

#include <string>
#include <string_view>

#include "base/memory/weak_ptr.h"

namespace captions {

// Caption Bubble Settings allows caption bubble to get and observe the caption
// bubble settings which can be set from chrome settings in case of live caption
// or from school tools UI in case of BabelOrca. It also allows storage and
// retrieval of the settings set by the user from the caption bubble itself.
class CaptionBubbleSettings {
 public:
  class Observer {
   public:
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    virtual ~Observer() = default;

    virtual void OnLiveTranslateEnabledChanged() = 0;
    virtual void OnLiveCaptionLanguageChanged() = 0;
    virtual void OnLiveTranslateTargetLanguageChanged() = 0;

   protected:
    Observer() = default;
  };
  CaptionBubbleSettings(const CaptionBubbleSettings&) = delete;
  CaptionBubbleSettings& operator=(const CaptionBubbleSettings&) = delete;

  virtual ~CaptionBubbleSettings() = default;

  virtual void SetObserver(base::WeakPtr<Observer> observer) = 0;
  virtual void RemoveObserver() = 0;

  virtual bool IsLiveTranslateFeatureEnabled() = 0;

  virtual bool GetLiveCaptionBubbleExpanded() = 0;
  virtual bool GetLiveTranslateEnabled() = 0;
  virtual std::string GetLiveCaptionLanguageCode() = 0;
  virtual std::string GetLiveTranslateTargetLanguageCode() = 0;

  virtual void SetLiveCaptionEnabled(bool enabled) = 0;
  virtual void SetLiveCaptionBubbleExpanded(bool expanded) = 0;
  virtual void SetLiveTranslateTargetLanguageCode(
      std::string_view language_code) = 0;

  virtual bool ShouldAdjustPositionOnExpand() = 0;

 protected:
  CaptionBubbleSettings() = default;
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_CAPTION_BUBBLE_SETTINGS_H_
