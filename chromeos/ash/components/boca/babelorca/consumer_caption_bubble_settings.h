// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_CONSUMER_CAPTION_BUBBLE_SETTINGS_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_CONSUMER_CAPTION_BUBBLE_SETTINGS_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_caption_bubble_settings.h"
#include "components/live_caption/caption_bubble_settings.h"

class PrefChangeRegistrar;
class PrefService;

namespace ash::babelorca {

class ConsumerCaptionBubbleSettings : public BabelOrcaCaptionBubbleSettings {
 public:
  ConsumerCaptionBubbleSettings(PrefService* profile_prefs,
                                std::string_view caption_language_code);

  ConsumerCaptionBubbleSettings(const ConsumerCaptionBubbleSettings&) = delete;
  ConsumerCaptionBubbleSettings& operator=(
      const ConsumerCaptionBubbleSettings&) = delete;

  ~ConsumerCaptionBubbleSettings() override;

  // BabelOrcaCaptionBubbleSettings:
  void SetObserver(base::WeakPtr<::captions::CaptionBubbleSettings::Observer>
                       observer) override;
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
  void SetLiveTranslateEnabled(bool enabled) override;

 private:
  const raw_ptr<PrefService> profile_prefs_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  bool translate_enabled_ = false;
  const std::string caption_language_code_;
  base::WeakPtr<::captions::CaptionBubbleSettings::Observer> observer_;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_CONSUMER_CAPTION_BUBBLE_SETTINGS_H_
