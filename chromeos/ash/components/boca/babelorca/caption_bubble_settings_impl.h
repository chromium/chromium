// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_CAPTION_BUBBLE_SETTINGS_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_CAPTION_BUBBLE_SETTINGS_IMPL_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/live_caption/caption_bubble_settings.h"

class PrefChangeRegistrar;
class PrefService;

namespace ash::babelorca {

class CaptionBubbleSettingsImpl : public ::captions::CaptionBubbleSettings {
 public:
  CaptionBubbleSettingsImpl(PrefService* profile_prefs,
                            std::string_view caption_language_code,
                            base::RepeatingClosure on_local_caption_closed_cb);

  CaptionBubbleSettingsImpl(const CaptionBubbleSettingsImpl&) = delete;
  CaptionBubbleSettingsImpl& operator=(const CaptionBubbleSettingsImpl&) =
      delete;

  ~CaptionBubbleSettingsImpl() override;

  // ::captions::CaptionBubbleSettings:
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
  bool ShouldAdjustPositionOnExpand() override;

  void SetLiveTranslateEnabled(bool enabled);
  void SetTranslateAllowed(bool allowed);
  bool GetTranslateAllowed();

 private:
  const raw_ptr<PrefService> profile_prefs_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  bool translate_allowed_ = true;
  bool translate_enabled_ = false;
  const std::string caption_language_code_;
  const base::RepeatingClosure on_local_caption_closed_cb_;
  base::WeakPtr<::captions::CaptionBubbleSettings::Observer> observer_;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_CAPTION_BUBBLE_SETTINGS_IMPL_H_
