// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APPLICATION_LOCALE_STORAGE_APPLICATION_LOCALE_STORAGE_H_
#define COMPONENTS_APPLICATION_LOCALE_STORAGE_APPLICATION_LOCALE_STORAGE_H_

#include "base/callback_list.h"
#include "base/i18n/language_tag.h"
#include "base/sequence_checker.h"

// Manages the locale used by the application. Must be used from the same
// sequence.
class ApplicationLocaleStorage {
 public:
  using OnLocaleChangedCallbackList =
      base::RepeatingCallbackList<void(const base::i18n::LanguageTag&)>;

  ApplicationLocaleStorage();
  ApplicationLocaleStorage(const ApplicationLocaleStorage&) = delete;
  ApplicationLocaleStorage& operator=(const ApplicationLocaleStorage&) = delete;
  ~ApplicationLocaleStorage();

  // Returns current locale language tag.
  const base::i18n::LanguageTag& GetTag() const;

  // Changes the locale language tag.
  void SetTag(base::i18n::LanguageTag new_locale);

  // Registers a callback which is triggered when the locale stored in this
  // class is changed.
  base::CallbackListSubscription RegisterOnLocaleChangedCallback(
      OnLocaleChangedCallbackList::CallbackType cb);

 private:
  base::i18n::LanguageTag locale_;
  OnLocaleChangedCallbackList on_locale_changed_callback_list_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // COMPONENTS_APPLICATION_LOCALE_STORAGE_APPLICATION_LOCALE_STORAGE_H_
