// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/consumer_caption_bubble_settings.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/boca/babelorca/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace ash::babelorca {

ConsumerCaptionBubbleSettings::ConsumerCaptionBubbleSettings(
    PrefService* profile_prefs,
    std::string_view caption_language_code)
    : profile_prefs_(profile_prefs),
      caption_language_code_(caption_language_code) {}

ConsumerCaptionBubbleSettings::~ConsumerCaptionBubbleSettings() = default;

void ConsumerCaptionBubbleSettings::SetObserver(
    base::WeakPtr<::captions::CaptionBubbleSettings::Observer> observer) {
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_prefs_);
  pref_change_registrar_->Add(
      prefs::kTranslateTargetLanguageCode,
      base::BindRepeating(&CaptionBubbleSettings::Observer::
                              OnLiveTranslateTargetLanguageChanged,
                          observer));
  observer_ = observer;
}

void ConsumerCaptionBubbleSettings::RemoveObserver() {
  pref_change_registrar_.reset();
  observer_.reset();
}

bool ConsumerCaptionBubbleSettings::IsLiveTranslateFeatureEnabled() {
  return translate_enabled_;
}

bool ConsumerCaptionBubbleSettings::GetLiveCaptionBubbleExpanded() {
  return profile_prefs_->GetBoolean(prefs::kCaptionBubbleExpanded);
}

bool ConsumerCaptionBubbleSettings::GetLiveTranslateEnabled() {
  return translate_enabled_;
}

std::string ConsumerCaptionBubbleSettings::GetLiveCaptionLanguageCode() {
  return caption_language_code_;
}

std::string
ConsumerCaptionBubbleSettings::GetLiveTranslateTargetLanguageCode() {
  return profile_prefs_->GetString(prefs::kTranslateTargetLanguageCode);
}

void ConsumerCaptionBubbleSettings::SetLiveCaptionEnabled(bool enabled) {}

void ConsumerCaptionBubbleSettings::SetLiveCaptionBubbleExpanded(
    bool expanded) {
  profile_prefs_->SetBoolean(prefs::kCaptionBubbleExpanded, expanded);
}

void ConsumerCaptionBubbleSettings::SetLiveTranslateTargetLanguageCode(
    std::string_view language_code) {
  profile_prefs_->SetString(prefs::kTranslateTargetLanguageCode, language_code);
}

void ConsumerCaptionBubbleSettings::SetLiveTranslateEnabled(bool enabled) {
  bool enabled_changed = translate_enabled_ != enabled;
  translate_enabled_ = enabled;
  if (!enabled_changed || !observer_) {
    return;
  }
  observer_->OnLiveTranslateEnabledChanged();
}

}  // namespace ash::babelorca
