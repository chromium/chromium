// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/live_caption_bubble_settings.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "components/live_caption/caption_bubble_settings.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "media/base/media_switches.h"

namespace captions {

LiveCaptionBubbleSettings::LiveCaptionBubbleSettings(PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {}

LiveCaptionBubbleSettings::~LiveCaptionBubbleSettings() = default;

void LiveCaptionBubbleSettings::SetObserver(
    base::WeakPtr<CaptionBubbleSettings::Observer> observer) {
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_prefs_);
  pref_change_registrar_->Add(
      prefs::kLiveTranslateEnabled,
      base::BindRepeating(
          &CaptionBubbleSettings::Observer::OnLiveTranslateEnabledChanged,
          observer));
  pref_change_registrar_->Add(
      prefs::kLiveCaptionLanguageCode,
      base::BindRepeating(
          &CaptionBubbleSettings::Observer::OnLiveCaptionLanguageChanged,
          observer));
  pref_change_registrar_->Add(
      prefs::kLiveTranslateTargetLanguageCode,
      base::BindRepeating(&CaptionBubbleSettings::Observer::
                              OnLiveTranslateTargetLanguageChanged,
                          observer));
}

void LiveCaptionBubbleSettings::RemoveObserver() {
  pref_change_registrar_.reset();
}

bool LiveCaptionBubbleSettings::IsLiveTranslateFeatureEnabled() {
  return media::IsLiveTranslateEnabled();
}

bool LiveCaptionBubbleSettings::GetLiveCaptionBubbleExpanded() {
  return profile_prefs_->GetBoolean(prefs::kLiveCaptionBubbleExpanded);
}

bool LiveCaptionBubbleSettings::GetLiveTranslateEnabled() {
  return profile_prefs_->GetBoolean(prefs::kLiveTranslateEnabled);
}

std::string LiveCaptionBubbleSettings::GetLiveCaptionLanguageCode() {
  return profile_prefs_->GetString(prefs::kLiveCaptionLanguageCode);
}

std::string LiveCaptionBubbleSettings::GetLiveTranslateTargetLanguageCode() {
  return profile_prefs_->GetString(prefs::kLiveTranslateTargetLanguageCode);
}

void LiveCaptionBubbleSettings::SetLiveCaptionEnabled(bool enabled) {
  profile_prefs_->SetBoolean(prefs::kLiveCaptionEnabled, enabled);
}

void LiveCaptionBubbleSettings::SetLiveCaptionBubbleExpanded(bool expanded) {
  profile_prefs_->SetBoolean(prefs::kLiveCaptionBubbleExpanded, expanded);
}

void LiveCaptionBubbleSettings::SetLiveTranslateTargetLanguageCode(
    std::string_view language_code) {
  profile_prefs_->SetString(prefs::kLiveTranslateTargetLanguageCode,
                            language_code);
}

bool LiveCaptionBubbleSettings::ShouldAdjustPositionOnExpand() {
  return false;
}

}  // namespace captions
