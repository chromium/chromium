// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_SCOPED_BROWSER_LOCALE_H_
#define CHROME_TEST_BASE_SCOPED_BROWSER_LOCALE_H_

#include <string>

// Helper class to temporarily set the locale of the browser process.
class ScopedBrowserLocale {
 public:
  explicit ScopedBrowserLocale(const std::string& new_locale);
  ScopedBrowserLocale(const ScopedBrowserLocale&) = delete;
  ScopedBrowserLocale& operator=(const ScopedBrowserLocale&) = delete;
  ~ScopedBrowserLocale();

 private:
  const std::string old_locale_;
};

#endif  // CHROME_TEST_BASE_SCOPED_BROWSER_LOCALE_H_
