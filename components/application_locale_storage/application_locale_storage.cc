// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/application_locale_storage/application_locale_storage.h"

#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "base/logging.h"
#include "base/sequence_checker.h"

ApplicationLocaleStorage::ApplicationLocaleStorage()
    : locale_(base::i18n::GetKnownLanguageTag("und")) {}

ApplicationLocaleStorage::~ApplicationLocaleStorage() = default;

const std::string& ApplicationLocaleStorage::Get(LocaleFormat) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tag_string_;
}

const base::i18n::LanguageTag& ApplicationLocaleStorage::GetTag() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return locale_;
}

void ApplicationLocaleStorage::Set(std::string new_locale) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<base::i18n::LanguageTag> new_locale_tag =
      base::i18n::GetLanguageTagFromString(new_locale);
  if (!new_locale_tag) {
    LOG(ERROR) << "Attempt to set new ApplicationLocaleStorage failed: "
               << new_locale;
    return;
  }
  SetTag(std::move(*new_locale_tag));
}

void ApplicationLocaleStorage::SetTag(base::i18n::LanguageTag new_locale) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  locale_ = std::move(new_locale);
  tag_string_ = std::string(locale_.tag_string());
  on_locale_changed_callback_list_.Notify(tag_string_);
}

base::CallbackListSubscription
ApplicationLocaleStorage::RegisterOnLocaleChangedCallback(
    OnLocaleChangedCallbackList::CallbackType cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return on_locale_changed_callback_list_.Add(std::move(cb));
}
