// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/application_locale_storage/application_locale_storage.h"

#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"

ApplicationLocaleStorage::ApplicationLocaleStorage() = default;

ApplicationLocaleStorage::~ApplicationLocaleStorage() = default;

const std::string& ApplicationLocaleStorage::Get(LocaleFormat format) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (format) {
    case LocaleFormat::kBCP47:
      return bcp47_locale_;
    case LocaleFormat::kChromeNormalized:
      return chrome_normalized_locale_;
  }
  NOTREACHED();
}

void ApplicationLocaleStorage::Set(std::string new_locale) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  chrome_normalized_locale_ = std::move(new_locale);
  bcp47_locale_ = chrome_normalized_locale_;
  base::ReplaceChars(bcp47_locale_, "_", "-", &bcp47_locale_);
  on_locale_changed_callback_list_.Notify(chrome_normalized_locale_);
}

base::CallbackListSubscription
ApplicationLocaleStorage::RegisterOnLocaleChangedCallback(
    OnLocaleChangedCallbackList::CallbackType cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return on_locale_changed_callback_list_.Add(std::move(cb));
}
