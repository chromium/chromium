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
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/live_caption/caption_bubble_context.h"
#include "components/live_caption/caption_bubble_settings.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/views/caption_bubble.h"
#include "components/live_caption/views/caption_bubble_model.h"
#include "components/live_caption/views/translation_view_wrapper.h"
#include "components/live_caption/views/translation_view_wrapper_base.h"
#include "components/prefs/pref_service.h"
#include "components/soda/soda_installer.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)
#include "components/on_device_translation/public/language_pack.h"
#include "components/on_device_translation/public/supported_languages.h"
#endif

namespace captions {

// Static
std::unique_ptr<CaptionBubbleController> CaptionBubbleController::Create(
    CaptionBubbleSettings* caption_bubble_settings,
    const std::string& application_locale,
    std::unique_ptr<TranslationViewWrapperBase> translation_view_wrapper) {
  return std::make_unique<CaptionBubbleControllerViews>(
      caption_bubble_settings, application_locale,
      std::move(translation_view_wrapper));
}

CaptionBubbleControllerViews::CaptionBubbleControllerViews(
    CaptionBubbleSettings* caption_bubble_settings,
    const std::string& application_locale,
    std::unique_ptr<TranslationViewWrapperBase> translation_view_wrapper)
    : application_locale_(application_locale) {
  caption_bubble_ = new CaptionBubble(
      caption_bubble_settings, std::move(translation_view_wrapper),
      application_locale,
      base::BindOnce(&CaptionBubbleControllerViews::OnCaptionBubbleDestroyed,
                     base::Unretained(this)));
  caption_widget_ =
      views::BubbleDialogDelegateView::CreateBubble(caption_bubble_);
  caption_bubble_->SetCaptionBubbleStyle();

#if !BUILDFLAG(IS_CHROMEOS)
  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  if (soda_installer) {
    soda_installer->AddObserver(this);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)
  auto* translation_installer =
      on_device_translation::OnDeviceTranslationInstaller::GetInstance();
  if (translation_installer) {
    translation_installer->AddObserver(this);
  }
#endif  // BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)
}

CaptionBubbleControllerViews::~CaptionBubbleControllerViews() {
  if (caption_widget_) {
    caption_widget_->CloseNow();
  }

#if !BUILDFLAG(IS_CHROMEOS)
  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  // `soda_installer` is not guaranteed to be valid, since it's possible for
  // this class to out-live it. This means that this class cannot use
  // ScopedObservation and needs to manage removing the observer itself.
  if (soda_installer) {
    soda_installer->RemoveObserver(this);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)
  auto* translation_installer =
      on_device_translation::OnDeviceTranslationInstaller::GetInstance();
  if (translation_installer) {
    translation_installer->RemoveObserver(this);
  }
#endif  // BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)
}

void CaptionBubbleControllerViews::OnCaptionBubbleDestroyed() {
  caption_bubble_ = nullptr;
  caption_widget_ = nullptr;
}

bool CaptionBubbleControllerViews::OnTranscription(
    content::RenderFrameHost* rfh,
    CaptionBubbleContext* caption_bubble_context,
    const media::SpeechRecognitionResult& result) {
  if (!caption_bubble_) {
    return false;
  }
  SetActiveModel(caption_bubble_context);
  if (active_model_->IsClosed()) {
    return false;
  }

  active_model_->SetPartialText(result.transcription);
  if (result.is_final) {
    active_model_->CommitPartialText();
  }

  return true;
}

void CaptionBubbleControllerViews::OnError(
    CaptionBubbleContext* caption_bubble_context,
    CaptionBubbleErrorType error_type,
    OnErrorClickedCallback error_clicked_callback,
    OnDoNotShowAgainClickedCallback error_silenced_callback) {
  if (!caption_bubble_) {
    return;
  }
  SetActiveModel(caption_bubble_context);
  if (active_model_->IsClosed()) {
    return;
  }
  active_model_->OnError(error_type, std::move(error_clicked_callback),
                         std::move(error_silenced_callback));
}

void CaptionBubbleControllerViews::OnAudioStreamEnd(
    content::RenderFrameHost* rfh,
    CaptionBubbleContext* caption_bubble_context) {
  if (!caption_bubble_) {
    return;
  }

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

    if (closed_sessions_.contains(caption_bubble_context->GetSessionId())) {
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
  if (closed_sessions_.contains(session_id)) {
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
  if (active_model_) {
    active_model_->Close();
  }
}

views::Widget* CaptionBubbleControllerViews::GetCaptionWidgetForTesting() {
  return caption_widget_;
}

CaptionBubble* CaptionBubbleControllerViews::GetCaptionBubbleForTesting() {
  return caption_bubble_;
}

void CaptionBubbleControllerViews::OnLanguageIdentificationEvent(
    content::RenderFrameHost* rfh,
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
  if (language_code != speech::LanguageCode::kNone) {
    soda_progress_ = std::nullopt;
    soda_language_code_ = speech::LanguageCode::kNone;
    if (active_model_) {
      UpdateCombinedProgressText();
    }
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
  if (language_code != speech::LanguageCode::kNone) {
    soda_progress_ = progress;
    soda_language_code_ = language_code;
    UpdateCombinedProgressText();
  }
}

#if BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)
void CaptionBubbleControllerViews::OnLanguagePackProgress(
    const on_device_translation::LanguagePackKey lang_pack,
    int progress) {
  translation_progress_[lang_pack] = progress;
  UpdateCombinedProgressText();
}

void CaptionBubbleControllerViews::OnLanguagePackInstalled(
    const on_device_translation::LanguagePackKey lang_pack) {
  translation_progress_.erase(lang_pack);
  UpdateCombinedProgressText();
}

void CaptionBubbleControllerViews::OnLanguagePackInstallationChanged(
    const on_device_translation::LanguagePackKey lang_pack) {
  auto* installer =
      on_device_translation::OnDeviceTranslationInstaller::GetInstance();
  if (installer && !installer->RegisteredLanguagePacks().contains(lang_pack) &&
      !installer->InstalledLanguagePacks().contains(lang_pack)) {
    translation_progress_.erase(lang_pack);
    UpdateCombinedProgressText();
  }
}
#endif  // BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)

void CaptionBubbleControllerViews::UpdateCombinedProgressText() {
  if (!active_model_) {
    return;
  }

  bool soda_is_downloading = soda_progress_ && *soda_progress_ < 100;
  bool soda_is_installing = soda_progress_ && *soda_progress_ >= 100;
  int downloading_translation_count = 0;
  int installing_count = soda_is_installing ? 1 : 0;
  int sum_progress = 0;

  if (soda_is_downloading) {
    sum_progress += *soda_progress_;
  }

#if BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)
  for (const auto& [lang_pack, progress] : translation_progress_) {
    if (progress < 100) {
      downloading_translation_count++;
      sum_progress += progress;
    } else {
      installing_count++;
    }
  }
#endif

  int total_downloading =
      (soda_is_downloading ? 1 : 0) + downloading_translation_count;

  if (total_downloading > 1) {
    int average_progress = sum_progress / total_downloading;
    active_model_->SetDownloadProgressText(l10n_util::GetStringFUTF16(
        IDS_LIVE_CAPTION_TRANSLATION_DOWNLOADING_LANGUAGE_PACKAGES_WITH_PROGRESS,
        base::UTF8ToUTF16(base::NumberToString(average_progress))));
    return;
  }

  std::vector<std::u16string> parts;

  if (soda_is_downloading) {
    std::u16string language_name = speech::GetLanguageDisplayName(
        speech::GetLanguageName(soda_language_code_), application_locale_);
    parts.push_back(l10n_util::GetStringFUTF16(
        IDS_LIVE_CAPTION_DOWNLOAD_PROGRESS, language_name,
        base::UTF8ToUTF16(base::NumberToString(*soda_progress_))));
  }

  std::u16string installing_language_name;
  if (soda_is_installing) {
    installing_language_name = speech::GetLanguageDisplayName(
        speech::GetLanguageName(soda_language_code_), application_locale_);
  }

#if BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)
  for (const auto& [lang_pack, progress] : translation_progress_) {
    if (progress < 0) {
      continue;
    }

    auto get_language_name = [&]() {
      return l10n_util::GetDisplayNameForLocale(
          std::string(on_device_translation::ToLanguageCode(
              on_device_translation::
                  NonEnglishSupportedLanguageFromLanguagePackKey(lang_pack))),
          application_locale_, /*is_for_ui=*/true);
    };

    if (progress < 100) {
      parts.push_back(l10n_util::GetStringFUTF16(
          IDS_LIVE_CAPTION_DOWNLOAD_PROGRESS, get_language_name(),
          base::UTF8ToUTF16(base::NumberToString(progress))));
    } else {
      installing_language_name = get_language_name();
    }
  }
#endif  // BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)

  if (total_downloading == 0 && installing_count > 0) {
    if (installing_count > 1) {
      parts.push_back(l10n_util::GetStringUTF16(
          IDS_LIVE_CAPTION_INSTALLING_LANGUAGE_PACKAGES));
    } else {
      parts.push_back(l10n_util::GetStringFUTF16(IDS_LIVE_CAPTION_INSTALLING,
                                                 installing_language_name));
    }
  }

  if (parts.empty()) {
    active_model_->OnLanguagePackInstalled();
  } else {
    active_model_->SetDownloadProgressText(base::JoinString(parts, u", "));
  }
}

}  // namespace captions
