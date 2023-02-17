// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_ui_delegate.h"

#include <algorithm>

#include "base/i18n/string_compare.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_experiments.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_util.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/url_util.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "third_party/metrics_proto/translate_event.pb.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kDeclineTranslate[] = "Translate.DeclineTranslate";
const char kRevertTranslation[] = "Translate.RevertTranslation";
const char kPerformTranslate[] = "Translate.Translate";
const char kPerformTranslateAmpCacheUrl[] = "Translate.Translate.AMPCacheURL";
const char kNeverTranslateLang[] = "Translate.NeverTranslateLang";
const char kNeverTranslateSite[] = "Translate.NeverTranslateSite";
const char kAlwaysTranslateLang[] = "Translate.AlwaysTranslateLang";
const char kModifySourceLang[] = "Translate.ModifyOriginalLang";
const char kModifyTargetLang[] = "Translate.ModifyTargetLang";
const char kShowErrorUI[] = "Translate.ShowErrorUI";

// Returns a Collator object which helps to sort strings in a given locale or
// null if unable to find the right collator.
//
// TODO(hajimehoshi): Write a test for icu::Collator::createInstance.
std::unique_ptr<icu::Collator> CreateCollator(const std::string& locale) {
  UErrorCode error = U_ZERO_ERROR;
  icu::Locale loc(locale.c_str());
  std::unique_ptr<icu::Collator> collator(
      icu::Collator::createInstance(loc, error));
  if (!collator || !U_SUCCESS(error))
    return nullptr;
  collator->setStrength(icu::Collator::PRIMARY);
  return collator;
}

// Returns whether |url| fits pattern of an AMP cache url.
// Note this is a copy of logic in amp_page_load_metrics_observer.cc
// TODO(crbug.com/1064974) Factor out into shared utility.
bool IsLikelyAmpCacheUrl(const GURL& url) {
  // Our heuristic to identify AMP cache URLs is to check for the presence of
  // the amp_js_v query param.
  for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    if (it.GetKey() == "amp_js_v")
      return true;
  }
  return false;
}

}  // namespace

namespace translate {

TranslateUIDelegate::TranslateUIDelegate(
    const base::WeakPtr<TranslateManager>& translate_manager,
    const std::string& source_language,
    const std::string& target_language)
    : translate_driver_(
          translate_manager->translate_client()->GetTranslateDriver()),
      translate_manager_(translate_manager),
      source_language_index_(kNoIndex),
      initial_source_language_index_(kNoIndex),
      target_language_index_(kNoIndex),
      prefs_(translate_manager_->translate_client()->GetTranslatePrefs()) {
  DCHECK(translate_driver_);
  DCHECK(translate_manager_);

  if (base::FeatureList::IsEnabled(
          language::kContentLanguagesInLanguagePicker)) {
    MaybeSetContentLanguages();

    if (!base::GetFieldTrialParamByFeatureAsBool(
            language::kContentLanguagesInLanguagePicker,
            language::kContentLanguagesDisableObserversParam,
            false /* default */)) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
      const std::string& pref_name = language::prefs::kPreferredLanguages;
#else
      const std::string& pref_name = language::prefs::kAcceptLanguages;
#endif

      // Also start listening for changes in the accept languages.
      PrefService* pref_service =
          translate_manager->translate_client()->GetPrefs();
      pref_change_registrar_.Init(pref_service);
      pref_change_registrar_.Add(
          pref_name,
          base::BindRepeating(&TranslateUIDelegate::MaybeSetContentLanguages,
                              base::Unretained(this)));
    }
  }

  std::string locale =
      TranslateDownloadManager::GetInstance()->application_locale();

  std::vector<std::string> language_codes;
  TranslateDownloadManager::GetSupportedLanguages(
      prefs_->IsTranslateAllowedByPolicy(), &language_codes);
  // Reserve additional space for unknown language option on all platforms
  // except iOS.
  std::vector<std::string>::size_type languages_size = language_codes.size();
#if !BUILDFLAG(IS_IOS)
  languages_size += 1;
#endif
  languages_.reserve(languages_size);

  // Preparing for the alphabetical order in the locale.
  std::unique_ptr<icu::Collator> collator = CreateCollator(locale);
  for (std::string& language_code : language_codes) {
    std::u16string language_name =
        l10n_util::GetDisplayNameForLocale(language_code, locale, true);
    languages_.emplace_back(std::move(language_code), std::move(language_name));
  }

  // Sort |languages_| in alphabetical order according to the display name.
  std::sort(
      languages_.begin(), languages_.end(),
      [&collator](const LanguageNamePair& lhs, const LanguageNamePair& rhs) {
        if (collator) {
          switch (base::i18n::CompareString16WithCollator(*collator, lhs.second,
                                                          rhs.second)) {
            case UCOL_LESS:
              return true;
            case UCOL_GREATER:
              return false;
            case UCOL_EQUAL:
              break;
          }
        } else {
          // |locale| may not be supported by ICU collator (crbug/54833). In
          // this case, let's order the languages in UTF-8.
          int result = lhs.second.compare(rhs.second);
          if (result != 0)
            return result < 0;
        }
        // Matching display names will be ordered alphabetically according to
        // the language codes.
        return lhs.first < rhs.first;
      });

  if (translate::IsForceTranslateEnabled()) {
    languages_.emplace_back(kUnknownLanguageCode,
                            GetUnknownLanguageDisplayName());
    std::rotate(languages_.rbegin(), languages_.rbegin() + 1,
                languages_.rend());
  }

  for (std::vector<LanguageNamePair>::const_iterator iter = languages_.begin();
       iter != languages_.end(); ++iter) {
    const std::string& language_code = iter->first;
    if (language_code == source_language) {
      source_language_index_ = iter - languages_.begin();
      initial_source_language_index_ = source_language_index_;
    }
    if (language_code == target_language)
      target_language_index_ = iter - languages_.begin();
  }
}

TranslateUIDelegate::~TranslateUIDelegate() = default;

void TranslateUIDelegate::MaybeSetContentLanguages() {
  std::string locale =
      TranslateDownloadManager::GetInstance()->application_locale();
  translatable_content_languages_codes_.clear();
  prefs_->GetTranslatableContentLanguages(
      locale, &translatable_content_languages_codes_);
}

void TranslateUIDelegate::OnErrorShown(TranslateErrors error_type) {
  DCHECK_LE(TranslateErrors::NONE, error_type);
  DCHECK_LT(error_type, TranslateErrors::TRANSLATE_ERROR_MAX);

  if (error_type == TranslateErrors::NONE)
    return;

  UMA_HISTOGRAM_ENUMERATION(kShowErrorUI, error_type,
                            TranslateErrors::TRANSLATE_ERROR_MAX);
}

const LanguageState* TranslateUIDelegate::GetLanguageState() {
  if (translate_manager_)
    return translate_manager_->GetLanguageState();
  return nullptr;
}

size_t TranslateUIDelegate::GetNumberOfLanguages() const {
  return languages_.size();
}

void TranslateUIDelegate::UpdateSourceLanguageIndex(size_t language_index) {
  DCHECK_LT(language_index, GetNumberOfLanguages());
  if (source_language_index_ == language_index)
    return;

  UMA_HISTOGRAM_BOOLEAN(kModifySourceLang, true);
  source_language_index_ = language_index;

  if (translate_manager_) {
    translate_manager_->GetActiveTranslateMetricsLogger()->LogSourceLanguage(
        GetLanguageCodeAt(language_index));
  }
}

void TranslateUIDelegate::UpdateSourceLanguage(
    const std::string& language_code) {
  if (GetSourceLanguageCode() == language_code) {
    return;
  }
  for (size_t i = 0; i < languages_.size(); ++i) {
    if (languages_[i].first.compare(language_code) == 0) {
      UpdateSourceLanguageIndex(i);
      if (translate_manager_) {
        translate_manager_->mutable_translate_event()
            ->set_modified_source_language(language_code);
      }
      return;
    }
  }
}

void TranslateUIDelegate::UpdateTargetLanguageIndex(size_t language_index) {
  DCHECK_LT(language_index, GetNumberOfLanguages());
  if (target_language_index_ == language_index)
    return;

  UMA_HISTOGRAM_BOOLEAN(kModifyTargetLang, true);
  target_language_index_ = language_index;

  if (translate_manager_) {
    translate_manager_->GetActiveTranslateMetricsLogger()->LogTargetLanguage(
        GetLanguageCodeAt(language_index),
        TranslateBrowserMetrics::TargetLanguageOrigin::kChangedByUser);
  }
}

void TranslateUIDelegate::UpdateTargetLanguage(
    const std::string& language_code) {
  if (GetTargetLanguageCode() == language_code) {
    return;
  }
  for (size_t i = 0; i < languages_.size(); ++i) {
    if (languages_[i].first.compare(language_code) == 0) {
      UpdateTargetLanguageIndex(i);
      if (translate_manager_) {
        translate_manager_->mutable_translate_event()
            ->set_modified_target_language(language_code);
      }
      return;
    }
  }
}

std::string TranslateUIDelegate::GetLanguageCodeAt(size_t index) const {
  DCHECK_LT(index, GetNumberOfLanguages());
  return languages_[index].first;
}

std::u16string TranslateUIDelegate::GetLanguageNameAt(size_t index) const {
  if (index == kNoIndex)
    return std::u16string();
  DCHECK_LT(index, GetNumberOfLanguages());
  return languages_[index].second;
}

void TranslateUIDelegate::GetContentLanguagesCodes(
    std::vector<std::string>* content_codes) const {
  DCHECK(content_codes != nullptr);
  content_codes->clear();

  for (auto& entry : translatable_content_languages_codes_) {
    content_codes->push_back(entry);
  }
}

std::string TranslateUIDelegate::GetSourceLanguageCode() const {
  return (GetSourceLanguageIndex() == kNoIndex)
             ? translate::kUnknownLanguageCode
             : GetLanguageCodeAt(GetSourceLanguageIndex());
}

std::string TranslateUIDelegate::GetTargetLanguageCode() const {
  return (GetTargetLanguageIndex() == kNoIndex)
             ? translate::kUnknownLanguageCode
             : GetLanguageCodeAt(GetTargetLanguageIndex());
}

void TranslateUIDelegate::Translate() {
  if (!translate_driver_->IsIncognito()) {
    prefs_->ResetTranslationDeniedCount(GetSourceLanguageCode());
    prefs_->ResetTranslationIgnoredCount(GetSourceLanguageCode());
    prefs_->IncrementTranslationAcceptedCount(GetSourceLanguageCode());
    prefs_->SetRecentTargetLanguage(GetTargetLanguageCode());
  }

  if (translate_manager_) {
    translate_manager_->RecordTranslateEvent(
        metrics::TranslateEventProto::USER_ACCEPT);
    translate_manager_->TranslatePage(
        GetSourceLanguageCode(), GetTargetLanguageCode(), false,
        translate_manager_->GetActiveTranslateMetricsLogger()
            ->GetNextManualTranslationType(
                /*is_context_menu_initiated_translation=*/false));
    UMA_HISTOGRAM_BOOLEAN(kPerformTranslate, true);
    if (IsLikelyAmpCacheUrl(translate_driver_->GetLastCommittedURL()))
      UMA_HISTOGRAM_BOOLEAN(kPerformTranslateAmpCacheUrl, true);
  }
}

void TranslateUIDelegate::RevertTranslation() {
  if (translate_manager_ &&
      translate_manager_->GetLanguageState()->IsPageTranslated()) {
    translate_manager_->RevertTranslation();
    UMA_HISTOGRAM_BOOLEAN(kRevertTranslation, true);
  }
}

void TranslateUIDelegate::TranslationDeclined(bool explicitly_closed) {
  if (!translate_driver_->IsIncognito()) {
    const std::string& language = GetSourceLanguageCode();
    if (explicitly_closed) {
      prefs_->ResetTranslationAcceptedCount(language);
      prefs_->IncrementTranslationDeniedCount(language);
    } else {
      prefs_->IncrementTranslationIgnoredCount(language);
    }
  }

  // Remember that the user declined the translation so as to prevent showing a
  // translate UI for that page again.  (TranslateManager initiates translations
  // when getting a LANGUAGE_DETERMINED from the page, which happens when a load
  // stops. That could happen multiple times, including after the user already
  // declined the translation.)
  if (translate_manager_) {
    translate_manager_->RecordTranslateEvent(
        explicitly_closed ? metrics::TranslateEventProto::USER_DECLINE
                          : metrics::TranslateEventProto::USER_IGNORE);
    if (explicitly_closed)
      translate_manager_->GetLanguageState()->set_translation_declined(true);
  }

  if (explicitly_closed) {
    UMA_HISTOGRAM_BOOLEAN(kDeclineTranslate, true);
  }
}

bool TranslateUIDelegate::IsLanguageBlocked() const {
  return prefs_->IsBlockedLanguage(GetSourceLanguageCode());
}

void TranslateUIDelegate::SetLanguageBlocked(bool value) {
  if (value) {
    prefs_->AddToLanguageList(GetSourceLanguageCode(),
                              /*force_blocked=*/true);
    if (translate_manager_) {
      // Translation has been blocked for this language. Capture that in the
      // metrics. Note that we don't capture a language being unblocked... which
      // is not the same as accepting a given translation for this language.
      translate_manager_->RecordTranslateEvent(
          metrics::TranslateEventProto::USER_NEVER_TRANSLATE_LANGUAGE);
    }
  } else {
    prefs_->UnblockLanguage(GetSourceLanguageCode());
  }

  UMA_HISTOGRAM_BOOLEAN(kNeverTranslateLang, value);
}

bool TranslateUIDelegate::IsSiteOnNeverPromptList() const {
  std::string host = GetPageHost();
  return !host.empty() && prefs_->IsSiteOnNeverPromptList(host);
}

bool TranslateUIDelegate::CanAddSiteToNeverPromptList() const {
  return !GetPageHost().empty();
}

void TranslateUIDelegate::SetNeverPromptSite(bool value) {
  std::string host = GetPageHost();
  if (host.empty())
    return;

  if (value) {
    prefs_->AddSiteToNeverPromptList(host);
    if (translate_manager_) {
      // Translation has been blocked for this site. Capture that in the metrics
      // Note that we don't capture a language being unblocked... which is not
      // the same as accepting a given translation for this site.
      translate_manager_->RecordTranslateEvent(
          metrics::TranslateEventProto::USER_NEVER_TRANSLATE_SITE);
    }
  } else {
    prefs_->RemoveSiteFromNeverPromptList(host);
  }

  UMA_HISTOGRAM_BOOLEAN(kNeverTranslateSite, value);
}

bool TranslateUIDelegate::ShouldAlwaysTranslate() const {
  return prefs_->IsLanguagePairOnAlwaysTranslateList(GetSourceLanguageCode(),
                                                     GetTargetLanguageCode());
}

bool TranslateUIDelegate::ShouldAlwaysTranslateBeCheckedByDefault() const {
  return ShouldAlwaysTranslate();
}

bool TranslateUIDelegate::ShouldShowAlwaysTranslateShortcut() const {
  return !translate_driver_->IsIncognito() &&
         prefs_->GetTranslationAcceptedCount(GetSourceLanguageCode()) >=
             kAlwaysTranslateShortcutMinimumAccepts;
}

bool TranslateUIDelegate::ShouldShowNeverTranslateShortcut() const {
  return !translate_driver_->IsIncognito() &&
         prefs_->GetTranslationDeniedCount(GetSourceLanguageCode()) >=
             kNeverTranslateShortcutMinimumDenials;
}

void TranslateUIDelegate::SetAlwaysTranslate(bool value) {
  const std::string& source_lang = GetSourceLanguageCode();
  const std::string& target_lang = GetTargetLanguageCode();
  if (value) {
    prefs_->AddLanguagePairToAlwaysTranslateList(source_lang, target_lang);
    // A default translation mapping has been accepted for this language.
    // Capture that in the metrics. Note that we don't capture a language being
    // unmapped... which is not the same as accepting some other translation
    // for this language.
    if (translate_manager_) {
      translate_manager_->RecordTranslateEvent(
          metrics::TranslateEventProto::USER_ALWAYS_TRANSLATE_LANGUAGE);
    }
    // If a language is being added to the always translate list on a
    // blocklisted site, remove that site from the blocklist.
    if (IsSiteOnNeverPromptList())
      SetNeverPromptSite(false);
  } else {
    prefs_->RemoveLanguagePairFromAlwaysTranslateList(source_lang, target_lang);
  }

  UMA_HISTOGRAM_BOOLEAN(kAlwaysTranslateLang, value);
}

std::string TranslateUIDelegate::GetPageHost() const {
  if (!translate_driver_->HasCurrentPage())
    return std::string();
  return translate_driver_->GetLastCommittedURL().HostNoBrackets();
}

void TranslateUIDelegate::OnUIClosedByUser() {
  if (translate_manager_)
    translate_manager_->GetActiveTranslateMetricsLogger()->LogUIChange(false);
}

void TranslateUIDelegate::ReportUIInteraction(UIInteraction ui_interaction) {
  if (translate_manager_) {
    translate_manager_->GetActiveTranslateMetricsLogger()->LogUIInteraction(
        ui_interaction);
  }
}

void TranslateUIDelegate::ReportUIChange(bool is_ui_shown) {
  if (translate_manager_) {
    translate_manager_->GetActiveTranslateMetricsLogger()->LogUIChange(
        is_ui_shown);
  }
}

// static
std::u16string TranslateUIDelegate::GetUnknownLanguageDisplayName() {
  return l10n_util::GetStringUTF16(IDS_TRANSLATE_DETECTED_LANGUAGE);
}

bool TranslateUIDelegate::IsIncognito() const {
  if (!translate_manager_)
    return false;
  TranslateClient* client = translate_manager_->translate_client();
  TranslateDriver* driver = client->GetTranslateDriver();
  return driver ? driver->IsIncognito() : false;
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

bool TranslateUIDelegate::ShouldAutoAlwaysTranslate() {
  // Don't trigger if it's off the record or already set to always translate.
  if (IsIncognito() || ShouldAlwaysTranslate())
    return false;

  const std::string& source_language = GetSourceLanguageCode();
  // Don't trigger for unknown source language.
  if (source_language == kUnknownLanguageCode)
    return false;

  bool always_translate =
      (prefs_->GetTranslationAcceptedCount(source_language) >=
           GetAutoAlwaysThreshold() &&
       prefs_->GetTranslationAutoAlwaysCount(source_language) <
           GetMaximumNumberOfAutoAlways());

  if (always_translate) {
    // Auto-always will be triggered. Need to increment the auto-always
    // counter.
    prefs_->IncrementTranslationAutoAlwaysCount(source_language);
    // Reset translateAcceptedCount so that auto-always could be triggered
    // again.
    prefs_->ResetTranslationAcceptedCount(source_language);
  }
  return always_translate;
}

bool TranslateUIDelegate::ShouldAutoNeverTranslate() {
  if (IsIncognito())
    return false;

  const std::string& source_language = GetSourceLanguageCode();
  // Don't trigger if this language is already blocked.
  if (!prefs_->CanTranslateLanguage(source_language))
    return false;

  int auto_never_count = prefs_->GetTranslationAutoNeverCount(source_language);

  // At the beginning (auto_never_count == 0), deniedCount starts at 0 and is
  // off-by-one (because this checking is done before increment). However,
  // after auto-never is triggered once (auto_never_count > 0), deniedCount
  // starts at 1. So there is no off-by-one by then.
  int off_by_one = auto_never_count == 0 ? 1 : 0;

  bool never_translate =
      (prefs_->GetTranslationDeniedCount(source_language) + off_by_one >=
           GetAutoNeverThreshold() &&
       auto_never_count < GetMaximumNumberOfAutoNever());
  if (never_translate) {
    // Auto-never will be triggered. Need to increment the auto-never counter.
    prefs_->IncrementTranslationAutoNeverCount(source_language);
    // Reset translateDeniedCount so that auto-never could be triggered again.
    prefs_->ResetTranslationDeniedCount(source_language);
  }
  return never_translate;
}

#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

}  // namespace translate
