// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/application_locale_storage/application_locale_storage.h"

#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/sequence_checker.h"

ApplicationLocaleStorage::ApplicationLocaleStorage() = default;

ApplicationLocaleStorage::~ApplicationLocaleStorage() = default;

const std::string& ApplicationLocaleStorage::Get() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return locale_;
}

void ApplicationLocaleStorage::Set(std::string new_locale) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  locale_ = std::move(new_locale);
  on_locale_changed_callback_list_.Notify(locale_);
}

base::CallbackListSubscription
ApplicationLocaleStorage::RegisterOnLocaleChangedCallback(
    OnLocaleChangedCallbackList::CallbackType cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return on_locale_changed_callback_list_.Add(std::move(cb));
}
