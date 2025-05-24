// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APPLICATION_LOCALE_STORAGE_APPLICATION_LOCALE_STORAGE_H_
#define COMPONENTS_APPLICATION_LOCALE_STORAGE_APPLICATION_LOCALE_STORAGE_H_

#include <string>

#include "base/callback_list.h"
#include "base/sequence_checker.h"

// Manages the locale used by the application. Must be used from the same
// sequence.
class ApplicationLocaleStorage {
 public:
  using OnLocaleChangedCallbackList =
      base::RepeatingCallbackList<void(const std::string&)>;

  ApplicationLocaleStorage();
  ApplicationLocaleStorage(const ApplicationLocaleStorage&) = delete;
  ApplicationLocaleStorage& operator=(const ApplicationLocaleStorage&) = delete;
  ~ApplicationLocaleStorage();

  // Returns current locale string.
  const std::string& Get() const;

  // Changes the locale string.
  void Set(std::string new_locale);

  // Registers a callback which is triggered when the locale stored in this
  // class is changed.
  base::CallbackListSubscription RegisterOnLocaleChangedCallback(
      OnLocaleChangedCallbackList::CallbackType cb);

 private:
  std::string locale_;
  OnLocaleChangedCallbackList on_locale_changed_callback_list_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // COMPONENTS_APPLICATION_LOCALE_STORAGE_APPLICATION_LOCALE_STORAGE_H_
