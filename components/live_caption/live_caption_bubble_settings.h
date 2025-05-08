// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_LIVE_CAPTION_BUBBLE_SETTINGS_H_
#define COMPONENTS_LIVE_CAPTION_LIVE_CAPTION_BUBBLE_SETTINGS_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/live_caption/caption_bubble_settings.h"

class PrefChangeRegistrar;
class PrefService;

namespace captions {

class LiveCaptionBubbleSettings : public CaptionBubbleSettings {
 public:
  explicit LiveCaptionBubbleSettings(PrefService* profile_prefs);

  LiveCaptionBubbleSettings(const LiveCaptionBubbleSettings&) = delete;
  LiveCaptionBubbleSettings& operator=(const LiveCaptionBubbleSettings&) =
      delete;

  ~LiveCaptionBubbleSettings() override;

  void SetObserver(
      base::WeakPtr<CaptionBubbleSettings::Observer> observer) override;
  void RemoveObserver() override;

  bool IsLiveTranslateFeatureEnabled() override;

  bool GetLiveCaptionBubbleExpanded() override;
  bool GetLiveTranslateEnabled() override;
  std::string GetLiveCaptionLanguageCode() override;
  std::string GetLiveTranslateTargetLanguageCode() override;

  void SetLiveCaptionEnabled(bool enabled) override;
  void SetLiveCaptionBubbleExpanded(bool expanded) override;
  void SetLiveTranslateTargetLanguageCode(
      std::string_view language_code) override;

  bool ShouldAdjustPositionOnExpand() override;

 private:
  const raw_ptr<PrefService> profile_prefs_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_LIVE_CAPTION_BUBBLE_SETTINGS_H_
