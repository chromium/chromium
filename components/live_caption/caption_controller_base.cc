// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/caption_controller_base.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "components/live_caption/caption_bubble_controller.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/pref_names.h"
#include "components/live_caption/views/translation_view_wrapper.h"
#include "components/live_caption/views/translation_view_wrapper_base.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

namespace captions {
namespace {

const char* const kCaptionStylePrefsToObserve[] = {
    prefs::kAccessibilityCaptionsTextSize,
    prefs::kAccessibilityCaptionsTextFont,
    prefs::kAccessibilityCaptionsTextColor,
    prefs::kAccessibilityCaptionsTextOpacity,
    prefs::kAccessibilityCaptionsBackgroundColor,
    prefs::kAccessibilityCaptionsTextShadow,
    prefs::kAccessibilityCaptionsBackgroundOpacity};

class CaptionControllerDelgateImpl : public CaptionControllerBase::Delegate {
 public:
  CaptionControllerDelgateImpl() = default;
  ~CaptionControllerDelgateImpl() override = default;

  std::unique_ptr<CaptionBubbleController> CreateCaptionBubbleController(
      CaptionBubbleSettings* caption_bubble_settings,
      const std::string& application_locale,
      std::unique_ptr<TranslationViewWrapperBase> translation_view_wrapper)
      override {
    return CaptionBubbleController::Create(caption_bubble_settings,
                                           application_locale,
                                           std::move(translation_view_wrapper));
  }

  void AddCaptionStyleObserver(ui::NativeThemeObserver* observer) override {
    ui::NativeTheme::GetInstanceForWeb()->AddObserver(observer);
  }

  void RemoveCaptionStyleObserver(ui::NativeThemeObserver* observer) override {
    ui::NativeTheme::GetInstanceForWeb()->RemoveObserver(observer);
  }
};

}  // namespace

CaptionControllerBase::~CaptionControllerBase() {
  // Both tests and production code may create the UI without destroying it
  // before reaching here. Ensure observers are deregistered properly. This is a
  // no-op if `!is_ui_constructed_`.
  DestroyUI();
}

CaptionControllerBase::CaptionControllerBase(
    PrefService* profile_prefs,
    const std::string& application_locale,
    std::unique_ptr<Delegate> delegate)
    : profile_prefs_(profile_prefs),
      application_locale_(application_locale),
      delegate_(delegate ? std::move(delegate)
                         : std::make_unique<CaptionControllerDelgateImpl>()),
      pref_change_registrar_(std::make_unique<PrefChangeRegistrar>()) {
  pref_change_registrar_->Init(profile_prefs_);

  // Turn off headless captioning when we first start, so that it does not get
  // stuck on.
  if (profile_prefs_->FindPreference(prefs::kHeadlessCaptionEnabled)) {
    profile_prefs_->SetBoolean(prefs::kHeadlessCaptionEnabled, false);
  }
}

void CaptionControllerBase::CreateUI() {
  if (is_ui_constructed_) {
    return;
  }

  is_ui_constructed_ = true;

  auto controller = delegate_->CreateCaptionBubbleController(
      caption_bubble_settings(), application_locale_,
      CreateTranslationViewWrapper());
  caption_bubble_controller_ = controller.get();
  AddListener(std::move(controller));

  // Observe native theme changes for caption style updates.
  delegate_->AddCaptionStyleObserver(this);

  // Observe caption style prefs.
  for (const char* const pref_name : kCaptionStylePrefsToObserve) {
    CHECK(!pref_change_registrar_->IsObserved(pref_name));
    pref_change_registrar_->Add(
        pref_name,
        base::BindRepeating(&CaptionControllerBase::OnCaptionStyleUpdated,
                            base::Unretained(this)));
  }
  OnCaptionStyleUpdated();
}

void CaptionControllerBase::DestroyUI() {
  if (!is_ui_constructed_) {
    return;
  }
  is_ui_constructed_ = false;

  RemoveListener(caption_bubble_controller_);
  CHECK(!caption_bubble_controller_);

  // Remove native theme observer.
  delegate_->RemoveCaptionStyleObserver(this);

  // Remove prefs to observe.
  for (const char* const pref_name : kCaptionStylePrefsToObserve) {
    CHECK(pref_change_registrar_->IsObserved(pref_name));
    pref_change_registrar_->Remove(pref_name);
  }
}

PrefService* CaptionControllerBase::profile_prefs() const {
  return profile_prefs_;
}

const std::string& CaptionControllerBase::application_locale() const {
  return application_locale_;
}

PrefChangeRegistrar* CaptionControllerBase::pref_change_registrar() const {
  return pref_change_registrar_.get();
}

CaptionBubbleController* CaptionControllerBase::caption_bubble_controller()
    const {
  return caption_bubble_controller_.get();
}

std::unique_ptr<TranslationViewWrapperBase>
CaptionControllerBase::CreateTranslationViewWrapper() {
  return std::make_unique<TranslationViewWrapper>(caption_bubble_settings());
}

void CaptionControllerBase::OnCaptionStyleUpdated() {
  if (!caption_bubble_controller_) {
    return;
  }
  // Metrics are recorded when passing the caption prefs to the browser, so do
  // not duplicate them here.
  std::optional<ui::CaptionStyle> caption_style =
      GetCaptionStyleFromUserSettings(profile_prefs_,
                                      /*record_metrics=*/false);
  caption_bubble_controller_->UpdateCaptionStyle(caption_style);
}

void CaptionControllerBase::AddListener(std::unique_ptr<Listener> listener) {
  listeners_.push_back(std::move(listener));
  if (listeners_.size() == 1) {
    OnFirstListenerAdded();
  }
}

void CaptionControllerBase::RemoveListener(Listener* listener) {
  if (caption_bubble_controller_ == listener) {
    caption_bubble_controller_ = nullptr;
  }
  // `std::find` doesn't like comparing unique_ptrs to raw ptrs.
  for (auto iter = listeners_.begin(); iter != listeners_.end(); iter++) {
    if (iter->get() != listener) {
      continue;
    }

    listeners_.erase(iter);

    if (listeners_.empty()) {
      OnLastListenerRemoved();
    }
    return;
  }
  NOTREACHED();
}

bool CaptionControllerBase::DispatchTranscription(
    content::RenderFrameHost* rfh,
    CaptionBubbleContext* caption_bubble_context,
    const media::SpeechRecognitionResult& result) {
  bool success = false;

  // Consider deleting the listener if it returns false.  It's unclear if
  // `caption_bubble_controller_` would allow this, but maybe.
  for (auto& listener : listeners_) {
    success |= listener->OnTranscription(rfh, caption_bubble_context, result);
  }

  return success;
}

void CaptionControllerBase::OnAudioStreamEnd(
    content::RenderFrameHost* rfh,
    CaptionBubbleContext* caption_bubble_context) {
  for (auto& listener : listeners_) {
    listener->OnAudioStreamEnd(rfh, caption_bubble_context);
  }
}

void CaptionControllerBase::OnLanguageIdentificationEvent(
    content::RenderFrameHost* rfh,
    CaptionBubbleContext* caption_bubble_context,
    const media::mojom::LanguageIdentificationEventPtr& event) {
  // TODO(crbug.com/40167928): Implement the UI for language identification.
  for (auto& listener : listeners_) {
    listener->OnLanguageIdentificationEvent(rfh, caption_bubble_context, event);
  }
}

}  // namespace captions
