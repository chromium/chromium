// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/live_translate_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/live_caption/features.h"
#include "components/live_caption/pref_names.h"
#include "components/live_caption/translation_dispatcher.h"
#include "components/live_caption/translation_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "google_apis/google_api_keys.h"
#include "media/mojo/mojom/speech_recognition_result.h"

namespace captions {
namespace {

constexpr char kLiveOnDeviceTranslateDispatcherResult[] =
    "Accessibility.LiveTranslate.OnDeviceTranslation.Result";
constexpr char kLiveGoogleApiTranslateDispatcherResult[] =
    "Accessibility.LiveTranslate.GoogleApiTranslation.Result";

}  // namespace

LiveTranslateController::LiveTranslateController(
    PrefService* profile_prefs,
    std::unique_ptr<TranslationDispatcher> on_device_dispatcher,
    std::unique_ptr<TranslationDispatcher> google_api_dispatcher)
    : profile_prefs_(profile_prefs),
      pref_change_registrar_(std::make_unique<PrefChangeRegistrar>()),
      on_device_dispatcher_(std::move(on_device_dispatcher)),
      google_api_dispatcher_(std::move(google_api_dispatcher)) {
  pref_change_registrar_->Init(profile_prefs_);
  pref_change_registrar_->Add(
      prefs::kLiveTranslateEnabled,
      base::BindRepeating(
          &LiveTranslateController::OnLiveTranslateEnabledChanged,
          // Unretained is safe because |this| owns |pref_change_registrar_|.
          base::Unretained(this)));
}

LiveTranslateController::~LiveTranslateController() = default;
// static
void LiveTranslateController::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kLiveTranslateEnabled, false);

  registry->RegisterStringPref(prefs::kLiveTranslateTargetLanguageCode,
                               speech::kEnglishLocaleNoCountry);
}

void LiveTranslateController::GetTranslation(const std::string& result,
                                             std::string source_language,
                                             std::string target_language,
                                             TranslateEventCallback callback) {
  base::UmaHistogramSparse(
      "Accessibility.LiveTranslate.GetTranslation.SourceLanguage",
      base::HashMetricName(
          speech::GetBCP47LanguageCodeFromSodaLanguage(source_language)
              .value_or(source_language)));
  base::UmaHistogramSparse(
      "Accessibility.LiveTranslate.GetTranslation.TargetLanguage",
      base::HashMetricName(target_language));

  if (base::FeatureList::IsEnabled(
          live_caption::kLiveCaptionOnDeviceTranslation)) {
    on_device_dispatcher_->GetTranslation(
        result, source_language, target_language,
        base::BindOnce(&LiveTranslateController::OnOnDeviceTranslated,
                       weak_factory_.GetWeakPtr(), result, source_language,
                       target_language, std::move(callback)));
  } else {
    google_api_dispatcher_->GetTranslation(
        result, source_language, target_language,
        base::BindOnce(&LiveTranslateController::OnGoogleApiTranslated,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void LiveTranslateController::OnOnDeviceTranslated(
    std::string_view result,
    std::string_view source_language,
    std::string_view target_language,
    TranslateEventCallback callback,
    const TranslateEvent& translate_event) {
  base::UmaHistogramBoolean(kLiveOnDeviceTranslateDispatcherResult,
                            translate_event.has_value());
  if (!translate_event.has_value() && google_api_dispatcher_) {
    google_api_dispatcher_->GetTranslation(
        std::string(result), std::string(source_language),
        std::string(target_language),
        base::BindOnce(&LiveTranslateController::OnGoogleApiTranslated,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  std::move(callback).Run(translate_event);
}

void LiveTranslateController::OnGoogleApiTranslated(
    TranslateEventCallback callback,
    const TranslateEvent& translate_event) {
  base::UmaHistogramBoolean(kLiveGoogleApiTranslateDispatcherResult,
                            translate_event.has_value());
  std::move(callback).Run(translate_event);
}

void LiveTranslateController::OnLiveTranslateEnabledChanged() {
  if (profile_prefs_->GetBoolean(prefs::kLiveTranslateEnabled))
    profile_prefs_->SetBoolean(prefs::kLiveCaptionEnabled, true);
}

}  // namespace captions
