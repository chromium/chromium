// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/views/caption_bubble_controller_views.h"

#include <memory>
#include <set>
#include <string>
#include <unordered_map>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/live_caption/caption_bubble_context.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/views/caption_bubble.h"
#include "components/live_caption/views/caption_bubble_model.h"
#include "components/prefs/pref_service.h"
#include "components/soda/soda_installer.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace captions {

// Static
std::unique_ptr<CaptionBubbleController> CaptionBubbleController::Create(
    PrefService* profile_prefs,
    const std::string& application_locale) {
  return std::make_unique<CaptionBubbleControllerViews>(profile_prefs,
                                                        application_locale);
}

CaptionBubbleControllerViews::CaptionBubbleControllerViews(
    PrefService* profile_prefs,
    const std::string& application_locale)
    : application_locale_(application_locale) {
  caption_bubble_ = new CaptionBubble(
      profile_prefs, application_locale,
      base::BindOnce(&CaptionBubbleControllerViews::OnCaptionBubbleDestroyed,
                     base::Unretained(this)));
  caption_widget_ =
      views::BubbleDialogDelegateView::CreateBubble(caption_bubble_);
  caption_bubble_->SetCaptionBubbleStyle();

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  if (soda_installer) {
    soda_installer->AddObserver(this);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

CaptionBubbleControllerViews::~CaptionBubbleControllerViews() {
  if (caption_widget_)
    caption_widget_->CloseNow();

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  // `soda_installer` is not guaranteed to be valid, since it's possible for
  // this class to out-live it. This means that this class cannot use
  // ScopedObservation and needs to manage removing the observer itself.
  if (soda_installer) {
    soda_installer->RemoveObserver(this);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void CaptionBubbleControllerViews::OnCaptionBubbleDestroyed() {
  caption_bubble_ = nullptr;
  caption_widget_ = nullptr;
}

bool CaptionBubbleControllerViews::OnTranscription(
    CaptionBubbleContext* caption_bubble_context,
    const media::SpeechRecognitionResult& result) {
  if (!caption_bubble_)
    return false;
  SetActiveModel(caption_bubble_context);
  if (active_model_->IsClosed())
    return false;

  // If the caption bubble has no activity and it receives a final
  // transcription, don't set text. The speech service sends a final
  // transcription after several seconds of no audio. This prevents the bubble
  // reappearing with a final transcription after it had disappeared due to no
  // activity.
  if (!caption_bubble_->HasActivity() && result.is_final)
    return true;

  active_model_->SetPartialText(result.transcription);
  if (result.is_final)
    active_model_->CommitPartialText();

  return true;
}

void CaptionBubbleControllerViews::OnError(
    CaptionBubbleContext* caption_bubble_context,
    CaptionBubbleErrorType error_type,
    OnErrorClickedCallback error_clicked_callback,
    OnDoNotShowAgainClickedCallback error_silenced_callback) {
  if (!caption_bubble_)
    return;
  SetActiveModel(caption_bubble_context);
  if (active_model_->IsClosed())
    return;
  active_model_->OnError(error_type, std::move(error_clicked_callback),
                         std::move(error_silenced_callback));
}

void CaptionBubbleControllerViews::OnAudioStreamEnd(
    CaptionBubbleContext* caption_bubble_context) {
  if (!caption_bubble_)
    return;

  auto caption_bubble_model_it =
      caption_bubble_models_.find(caption_bubble_context);
  if (caption_bubble_model_it == caption_bubble_models_.end()) {
    return;
  }
  if (active_model_ != nullptr &&
      active_model_->unique_id() ==
          caption_bubble_model_it->second->unique_id()) {
    active_model_ = nullptr;
    caption_bubble_->SetModel(nullptr);
  }
  caption_bubble_models_.erase(caption_bubble_model_it);
}

void CaptionBubbleControllerViews::UpdateCaptionStyle(
    std::optional<ui::CaptionStyle> caption_style) {
  caption_bubble_->UpdateCaptionStyle(caption_style);
}

void CaptionBubbleControllerViews::SetActiveModel(
    CaptionBubbleContext* caption_bubble_context) {
  auto caption_bubble_model_it =
      caption_bubble_models_.find(caption_bubble_context);
  if (caption_bubble_model_it == caption_bubble_models_.end()) {
    auto caption_bubble_model = std::make_unique<CaptionBubbleModel>(
        caption_bubble_context,
        base::BindRepeating(
            &CaptionBubbleControllerViews::OnSessionEnded,
            // Unretained is safe because |CaptionBubbleControllerViews|
            // owns |caption_bubble_model|.
            base::Unretained(this)));

    if (base::Contains(closed_sessions_,
                       caption_bubble_context->GetSessionId())) {
      caption_bubble_model->Close();
    }

    caption_bubble_model_it =
        caption_bubble_models_
            .emplace(caption_bubble_context, std::move(caption_bubble_model))
            .first;
  }

  if (!caption_bubble_session_observers_.count(
          caption_bubble_context->GetSessionId())) {
    std::unique_ptr<CaptionBubbleSessionObserver> observer =
        caption_bubble_context->GetCaptionBubbleSessionObserver();

    if (observer) {
      observer->SetEndSessionCallback(
          base::BindRepeating(&CaptionBubbleControllerViews::OnSessionReset,
                              base::Unretained(this)));
      caption_bubble_session_observers_.emplace(
          caption_bubble_context->GetSessionId(), std::move(observer));
    }
  }

  if (active_model_ == nullptr ||
      active_model_->unique_id() !=
          caption_bubble_model_it->second->unique_id()) {
    active_model_ = caption_bubble_model_it->second.get();
    caption_bubble_->SetModel(active_model_);
  }
}

void CaptionBubbleControllerViews::OnSessionEnded(
    const std::string& session_id) {
  // Close all other CaptionBubbleModels that share this WebContents identifier.
  for (const auto& caption_bubble_model : caption_bubble_models_) {
    if (caption_bubble_model.first->GetSessionId() == session_id) {
      caption_bubble_model.second->Close();
    }
  }

  closed_sessions_.insert(session_id);
}

void CaptionBubbleControllerViews::OnSessionReset(
    const std::string& session_id) {
  if (base::Contains(closed_sessions_, session_id)) {
    closed_sessions_.erase(session_id);
  }

  caption_bubble_session_observers_.erase(session_id);
}

bool CaptionBubbleControllerViews::IsWidgetVisibleForTesting() {
  return caption_widget_ && caption_widget_->IsVisible();
}

bool CaptionBubbleControllerViews::IsGenericErrorMessageVisibleForTesting() {
  return caption_bubble_ &&
         caption_bubble_->IsGenericErrorMessageVisibleForTesting();  // IN-TEST
}

std::string CaptionBubbleControllerViews::GetBubbleLabelTextForTesting() {
  return caption_bubble_
             ? base::UTF16ToUTF8(
                   caption_bubble_->GetLabelForTesting()->GetText())  // IN-TEST
             : "";
}

void CaptionBubbleControllerViews::CloseActiveModelForTesting() {
  if (active_model_)
    active_model_->Close();
}

views::Widget* CaptionBubbleControllerViews::GetCaptionWidgetForTesting() {
  return caption_widget_;
}

CaptionBubble* CaptionBubbleControllerViews::GetCaptionBubbleForTesting() {
  return caption_bubble_;
}

void CaptionBubbleControllerViews::OnLanguageIdentificationEvent(
    CaptionBubbleContext* caption_bubble_context,
    const media::mojom::LanguageIdentificationEventPtr& event) {
  if (!caption_bubble_) {
    return;
  }
  SetActiveModel(caption_bubble_context);
  if (active_model_->IsClosed()) {
    return;
  }

  if (event->asr_switch_result ==
      media::mojom::AsrSwitchResult::kSwitchSucceeded) {
    active_model_->SetLanguage(event->language);
  }
}

void CaptionBubbleControllerViews::OnSodaInstalled(
    speech::LanguageCode language_code) {
  if (active_model_ && language_code != speech::LanguageCode::kNone) {
    active_model_->OnLanguagePackInstalled();
  }
}

void CaptionBubbleControllerViews::OnSodaInstallError(
    speech::LanguageCode language_code,
    speech::SodaInstaller::ErrorCode error_code) {
  if (active_model_ && language_code != speech::LanguageCode::kNone) {
    active_model_->SetDownloadProgressText(l10n_util::GetStringFUTF16(
        IDS_LIVE_CAPTION_LANGUAGE_DOWNLOAD_FAILED,
        speech::GetLanguageDisplayName(speech::GetLanguageName(language_code),
                                       application_locale_)));
  }
}

void CaptionBubbleControllerViews::OnSodaProgress(
    speech::LanguageCode language_code,
    int progress) {
  if (active_model_ && language_code != speech::LanguageCode::kNone) {
    active_model_->SetDownloadProgressText(l10n_util::GetStringFUTF16(
        IDS_LIVE_CAPTION_DOWNLOAD_PROGRESS,
        speech::GetLanguageDisplayName(speech::GetLanguageName(language_code),
                                       application_locale_),
        base::UTF8ToUTF16(base::NumberToString(progress))));
  }
}

}  // namespace captions
