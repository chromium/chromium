// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/caption_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/boca/babelorca/caption_bubble_settings_impl.h"
#include "chromeos/ash/components/boca/babelorca/translation_view_wrapper_impl.h"
#include "components/live_caption/caption_bubble_context.h"
#include "components/live_caption/caption_bubble_controller.h"
#include "components/live_caption/caption_controller_base.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/live_caption_bubble_settings.h"
#include "components/live_caption/pref_names.h"
#include "components/live_caption/views/translation_view_wrapper_base.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "ui/native_theme/caption_style.h"
#include "ui/native_theme/native_theme_observer.h"

namespace ash::babelorca {

CaptionController::CaptionController(
    std::unique_ptr<::captions::CaptionBubbleContext> caption_bubble_context,
    PrefService* profile_prefs,
    const std::string& application_locale,
    std::unique_ptr<CaptionBubbleSettingsImpl> caption_bubble_settings,
    std::unique_ptr<Delegate> delegate)
    : ::captions::CaptionControllerBase(profile_prefs,
                                        application_locale,
                                        std::move(delegate)),
      caption_bubble_context_(std::move(caption_bubble_context)),
      caption_bubble_settings_(std::move(caption_bubble_settings)) {}

CaptionController::~CaptionController() {
  StopLiveCaption();
}

void CaptionController::StartLiveCaption() {
  CreateUI();
}

void CaptionController::StopLiveCaption() {
  DestroyUI();
}

void CaptionController::SetLiveTranslateEnabled(bool enabled) {
  caption_bubble_settings_->SetLiveTranslateEnabled(enabled);
}

void CaptionController::SetTranslateAllowed(bool allowed) {
  caption_bubble_settings_->SetTranslateAllowed(allowed);
}

bool CaptionController::IsTranslateAllowedAndEnabled() {
  return caption_bubble_settings_->GetLiveTranslateEnabled();
}

std::string CaptionController::GetLiveTranslateTargetLanguageCode() {
  return caption_bubble_settings_->GetLiveTranslateTargetLanguageCode();
}

bool CaptionController::DispatchTranscription(
    const media::SpeechRecognitionResult& result) {
  if (!caption_bubble_controller()) {
    return false;
  }
  return caption_bubble_controller()->OnTranscription(
      /*web_contents=*/nullptr, caption_bubble_context_.get(), result);
}

void CaptionController::OnLanguageIdentificationEvent(
    const media::mojom::LanguageIdentificationEventPtr& event) {
  if (!caption_bubble_controller()) {
    return;
  }
  caption_bubble_controller()->OnLanguageIdentificationEvent(
      /*web_contents=*/nullptr, caption_bubble_context_.get(), event);
}

captions::CaptionBubbleSettings* CaptionController::caption_bubble_settings() {
  return caption_bubble_settings_.get();
}

std::unique_ptr<captions::TranslationViewWrapperBase>
CaptionController::CreateTranslationViewWrapper() {
  if (!features::IsBocaTranslateToggleEnabled()) {
    return ::captions::CaptionControllerBase::CreateTranslationViewWrapper();
  }
  return std::make_unique<TranslationViewWrapperImpl>(
      caption_bubble_settings_.get());
}

}  // namespace ash::babelorca
