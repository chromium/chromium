// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/caption_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "components/live_caption/caption_bubble_context.h"
#include "components/live_caption/caption_bubble_controller.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "ui/native_theme/caption_style.h"
#include "ui/native_theme/native_theme_observer.h"

namespace ash::babelorca {
namespace {

const char* const kCaptionStylePrefsToObserve[] = {
    prefs::kAccessibilityCaptionsTextSize,
    prefs::kAccessibilityCaptionsTextFont,
    prefs::kAccessibilityCaptionsTextColor,
    prefs::kAccessibilityCaptionsTextOpacity,
    prefs::kAccessibilityCaptionsBackgroundColor,
    prefs::kAccessibilityCaptionsTextShadow,
    prefs::kAccessibilityCaptionsBackgroundOpacity};

class CaptionControllerDelgateImpl : public CaptionController::Delegate {
 public:
  CaptionControllerDelgateImpl() = default;
  ~CaptionControllerDelgateImpl() override = default;

  std::unique_ptr<::captions::CaptionBubbleController>
  CreateCaptionBubbleController(
      PrefService* profile_prefs,
      const std::string& application_locale) override {
    return ::captions::CaptionBubbleController::Create(profile_prefs,
                                                       application_locale);
  }

  void AddCaptionStyleObserver(ui::NativeThemeObserver* observer) override {
    ui::NativeTheme::GetInstanceForWeb()->AddObserver(observer);
  }

  void RemoveCaptionStyleObserver(ui::NativeThemeObserver* observer) override {
    ui::NativeTheme::GetInstanceForWeb()->RemoveObserver(observer);
  }
};

}  // namespace

CaptionController::CaptionController(
    std::unique_ptr<::captions::CaptionBubbleContext> caption_bubble_context,
    PrefService* profile_prefs,
    const std::string& application_locale,
    std::unique_ptr<Delegate> delegate)
    : caption_bubble_context_(std::move(caption_bubble_context)),
      profile_prefs_(profile_prefs),
      application_locale_(application_locale),
      delegate_(delegate ? std::move(delegate)
                         : std::make_unique<CaptionControllerDelgateImpl>()) {
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_prefs_);
}

CaptionController::~CaptionController() {
  StopLiveCaption();
}

void CaptionController::StartLiveCaption() {
  if (is_ui_constructed_) {
    return;
  }

  is_ui_constructed_ = true;
  caption_bubble_controller_ = delegate_->CreateCaptionBubbleController(
      profile_prefs_, application_locale_);
  OnCaptionStyleUpdated();
  // Observe native theme changes for caption style updates.
  delegate_->AddCaptionStyleObserver(this);
  // Observe caption style prefs.
  for (const char* const pref_name : kCaptionStylePrefsToObserve) {
    pref_change_registrar_->Add(
        pref_name,
        base::BindRepeating(&CaptionController::OnCaptionStyleUpdated,
                            base::Unretained(this)));
  }
}

void CaptionController::StopLiveCaption() {
  if (!is_ui_constructed_) {
    return;
  }
  is_ui_constructed_ = false;
  caption_bubble_controller_.reset();
  // Remove native theme observer.
  delegate_->RemoveCaptionStyleObserver(this);
  // Remove prefs to observe.
  for (const char* const pref_name : kCaptionStylePrefsToObserve) {
    pref_change_registrar_->Remove(pref_name);
  }
}

bool CaptionController::DispatchTranscription(
    const media::SpeechRecognitionResult& result) {
  if (!caption_bubble_controller_) {
    return false;
  }
  bool success = caption_bubble_controller_->OnTranscription(
      caption_bubble_context_.get(), result);
  if (success) {
    return true;
  }
  // Rebuild caption bubble in case it was closed.
  caption_bubble_controller_ = delegate_->CreateCaptionBubbleController(
      profile_prefs_, application_locale_);
  return caption_bubble_controller_->OnTranscription(
      caption_bubble_context_.get(), result);
}

void CaptionController::OnAudioStreamEnd() {
  if (!caption_bubble_controller_) {
    return;
  }
  caption_bubble_controller_->OnAudioStreamEnd(caption_bubble_context_.get());
}

void CaptionController::OnCaptionStyleUpdated() {
  caption_style_ = ::captions::GetCaptionStyleFromUserSettings(
      profile_prefs_, false /* record_metrics */);
  caption_bubble_controller_->UpdateCaptionStyle(caption_style_);
}

}  // namespace ash::babelorca
