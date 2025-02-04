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
      const std::string& application_locale) override {
    return CaptionBubbleController::Create(caption_bubble_settings,
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

CaptionControllerBase::~CaptionControllerBase() = default;

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
}

void CaptionControllerBase::CreateUI() {
  if (is_ui_constructed_) {
    return;
  }

  is_ui_constructed_ = true;

  caption_bubble_controller_ = delegate_->CreateCaptionBubbleController(
      caption_bubble_settings(), application_locale_);

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

  caption_bubble_controller_.reset(nullptr);

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

}  // namespace captions
