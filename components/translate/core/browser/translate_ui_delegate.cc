// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_ui_delegate.h"

#include <algorithm>

#include "base/i18n/string_compare.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/translate_constants.h"
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
const char kModifyOriginalLang[] = "Translate.ModifyOriginalLang";
const char kModifyTargetLang[] = "Translate.ModifyTargetLang";
const char kDeclineTranslateDismissUI[] = "Translate.DeclineTranslateDismissUI";
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
    const std::string& original_language,
    const std::string& target_language)
    : translate_driver_(
          translate_manager->translate_client()->GetTranslateDriver()),
      translate_manager_(translate_manager),
      original_language_index_(kNoIndex),
      initial_original_language_index_(kNoIndex),
      target_language_index_(kNoIndex),
      prefs_(translate_manager_->translate_client()->GetTranslatePrefs()) {
  DCHECK(translate_driver_);
  DCHECK(translate_manager_);

  std::vector<std::string> language_codes;
  TranslateDownloadManager::GetSupportedLanguages(
      prefs_->IsTranslateAllowedByPolicy(), &language_codes);

  // Preparing for the alphabetical order in the locale.
  std::string locale =
      TranslateDownloadManager::GetInstance()->application_locale();
  std::unique_ptr<icu::Collator> collator = CreateCollator(locale);

  languages_.reserve(language_codes.size());
  for (std::string& language_code : language_codes) {
    base::string16 language_name =
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

  for (std::vector<LanguageNamePair>::const_iterator iter = languages_.begin();
       iter != languages_.end(); ++iter) {
    const std::string& language_code = iter->first;
    if (language_code == original_language) {
      original_language_index_ = iter - languages_.begin();
      initial_original_language_index_ = original_language_index_;
    }
    if (language_code == target_language)
      target_language_index_ = iter - languages_.begin();
  }
}

TranslateUIDelegate::~TranslateUIDelegate() {}

void TranslateUIDelegate::OnErrorShown(TranslateErrors::Type error_type) {
  DCHECK_LE(TranslateErrors::NONE, error_type);
  DCHECK_LT(error_type, TranslateErrors::TRANSLATE_ERROR_MAX);

  if (error_type == TranslateErrors::NONE)
    return;

  UMA_HISTOGRAM_ENUMERATION(kShowErrorUI, error_type,
                            TranslateErrors::TRANSLATE_ERROR_MAX);
}

const LanguageState& TranslateUIDelegate::GetLanguageState() {
  return *translate_manager_->GetLanguageState();
}

size_t TranslateUIDelegate::GetNumberOfLanguages() const {
  return languages_.size();
}

void TranslateUIDelegate::UpdateOriginalLanguageIndex(size_t language_index) {
  if (original_language_index_ == language_index)
    return;

  UMA_HISTOGRAM_BOOLEAN(kModifyOriginalLang, true);
  original_language_index_ = language_index;
}

void TranslateUIDelegate::UpdateOriginalLanguage(
    const std::string& language_code) {
  DCHECK(translate_manager_ != nullptr);
  for (size_t i = 0; i < languages_.size(); ++i) {
    if (languages_[i].first.compare(language_code) == 0) {
      UpdateOriginalLanguageIndex(i);
      translate_manager_->mutable_translate_event()
          ->set_modified_source_language(language_code);
      return;
    }
  }
}

void TranslateUIDelegate::UpdateTargetLanguageIndex(size_t language_index) {
  if (target_language_index_ == language_index)
    return;

  DCHECK_LT(language_index, GetNumberOfLanguages());
  UMA_HISTOGRAM_BOOLEAN(kModifyTargetLang, true);
  target_language_index_ = language_index;
}

void TranslateUIDelegate::UpdateTargetLanguage(
    const std::string& language_code) {
  DCHECK(translate_manager_ != nullptr);
  for (size_t i = 0; i < languages_.size(); ++i) {
    if (languages_[i].first.compare(language_code) == 0) {
      UpdateTargetLanguageIndex(i);
      translate_manager_->mutable_translate_event()
          ->set_modified_target_language(language_code);
      return;
    }
  }
}

std::string TranslateUIDelegate::GetLanguageCodeAt(size_t index) const {
  DCHECK_LT(index, GetNumberOfLanguages());
  return languages_[index].first;
}

base::string16 TranslateUIDelegate::GetLanguageNameAt(size_t index) const {
  if (index == kNoIndex)
    return base::string16();
  DCHECK_LT(index, GetNumberOfLanguages());
  return languages_[index].second;
}

std::string TranslateUIDelegate::GetOriginalLanguageCode() const {
  return (GetOriginalLanguageIndex() == kNoIndex)
             ? translate::kUnknownLanguageCode
             : GetLanguageCodeAt(GetOriginalLanguageIndex());
}

std::string TranslateUIDelegate::GetTargetLanguageCode() const {
  return (GetTargetLanguageIndex() == kNoIndex)
             ? translate::kUnknownLanguageCode
             : GetLanguageCodeAt(GetTargetLanguageIndex());
}

void TranslateUIDelegate::Translate() {
  if (!translate_driver_->IsIncognito()) {
    prefs_->ResetTranslationDeniedCount(GetOriginalLanguageCode());
    prefs_->ResetTranslationIgnoredCount(GetOriginalLanguageCode());
    prefs_->IncrementTranslationAcceptedCount(GetOriginalLanguageCode());
    prefs_->SetRecentTargetLanguage(GetTargetLanguageCode());
  }

  if (translate_manager_) {
    translate_manager_->RecordTranslateEvent(
        metrics::TranslateEventProto::USER_ACCEPT);
    translate_manager_->TranslatePage(GetOriginalLanguageCode(),
                                      GetTargetLanguageCode(), false);
    UMA_HISTOGRAM_BOOLEAN(kPerformTranslate, true);
    if (IsLikelyAmpCacheUrl(translate_driver_->GetLastCommittedURL()))
      UMA_HISTOGRAM_BOOLEAN(kPerformTranslateAmpCacheUrl, true);
  }
}

void TranslateUIDelegate::RevertTranslation() {
  if (translate_manager_) {
    translate_manager_->RevertTranslation();
    UMA_HISTOGRAM_BOOLEAN(kRevertTranslation, true);
  }
}

void TranslateUIDelegate::TranslationDeclined(bool explicitly_closed) {
  if (!translate_driver_->IsIncognito()) {
    const std::string& language = GetOriginalLanguageCode();
    if (explicitly_closed) {
      prefs_->ResetTranslationAcceptedCount(language);
      prefs_->IncrementTranslationDeniedCount(language);
      prefs_->UpdateLastDeniedTime(language);
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
  } else {
    UMA_HISTOGRAM_BOOLEAN(kDeclineTranslateDismissUI, true);
  }
}

bool TranslateUIDelegate::IsLanguageBlocked() const {
  return prefs_->IsBlockedLanguage(GetOriginalLanguageCode());
}

void TranslateUIDelegate::SetLanguageBlocked(bool value) {
  if (value) {
    prefs_->AddToLanguageList(GetOriginalLanguageCode(),
                              /*force_blocked=*/true);
    if (translate_manager_) {
      // Translation has been blocked for this language. Capture that in the
      // metrics. Note that we don't capture a language being unblocked... which
      // is not the same as accepting a given translation for this language.
      translate_manager_->RecordTranslateEvent(
          metrics::TranslateEventProto::USER_NEVER_TRANSLATE_LANGUAGE);
    }
  } else {
    prefs_->UnblockLanguage(GetOriginalLanguageCode());
  }

  UMA_HISTOGRAM_BOOLEAN(kNeverTranslateLang, value);
}

bool TranslateUIDelegate::IsSiteBlacklisted() const {
  std::string host = GetPageHost();
  return !host.empty() && prefs_->IsSiteBlacklisted(host);
}

bool TranslateUIDelegate::CanBlacklistSite() const {
  return !GetPageHost().empty();
}

void TranslateUIDelegate::SetSiteBlacklist(bool value) {
  std::string host = GetPageHost();
  if (host.empty())
    return;

  if (value) {
    prefs_->BlacklistSite(host);
    if (translate_manager_) {
      // Translation has been blocked for this site. Capture that in the metrics
      // Note that we don't capture a language being unblocked... which is not
      // the same as accepting a given translation for this site.
      translate_manager_->RecordTranslateEvent(
          metrics::TranslateEventProto::USER_NEVER_TRANSLATE_SITE);
    }
  } else {
    prefs_->RemoveSiteFromBlacklist(host);
  }

  UMA_HISTOGRAM_BOOLEAN(kNeverTranslateSite, value);
}

bool TranslateUIDelegate::ShouldAlwaysTranslate() const {
  return prefs_->IsLanguagePairWhitelisted(GetOriginalLanguageCode(),
                                           GetTargetLanguageCode());
}

bool TranslateUIDelegate::ShouldAlwaysTranslateBeCheckedByDefault() const {
  return ShouldAlwaysTranslate();
}

bool TranslateUIDelegate::ShouldShowAlwaysTranslateShortcut() const {
  return !translate_driver_->IsIncognito() &&
         prefs_->GetTranslationAcceptedCount(GetOriginalLanguageCode()) >=
             kAlwaysTranslateShortcutMinimumAccepts;
}

bool TranslateUIDelegate::ShouldShowNeverTranslateShortcut() const {
  return !translate_driver_->IsIncognito() &&
         prefs_->GetTranslationDeniedCount(GetOriginalLanguageCode()) >=
             kNeverTranslateShortcutMinimumDenials;
}

void TranslateUIDelegate::SetAlwaysTranslate(bool value) {
  const std::string& original_lang = GetOriginalLanguageCode();
  const std::string& target_lang = GetTargetLanguageCode();
  if (value) {
    prefs_->WhitelistLanguagePair(original_lang, target_lang);
    // A default translation mapping has been accepted for this language.
    // Capture that in the metrics. Note that we don't capture a language being
    // unmapped... which is not the same as accepting some other translation
    // for this language.
    if (translate_manager_) {
      translate_manager_->RecordTranslateEvent(
          metrics::TranslateEventProto::USER_ALWAYS_TRANSLATE_LANGUAGE);
    }
  } else {
    prefs_->RemoveLanguagePairFromWhitelist(original_lang, target_lang);
  }

  UMA_HISTOGRAM_BOOLEAN(kAlwaysTranslateLang, value);
}

std::string TranslateUIDelegate::GetPageHost() const {
  if (!translate_driver_->HasCurrentPage())
    return std::string();
  return translate_driver_->GetLastCommittedURL().HostNoBrackets();
}

}  // namespace translate
