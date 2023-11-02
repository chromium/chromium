// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/scoped_browser_locale.h"

#include "chrome/browser/browser_process.h"

ScopedBrowserLocale::ScopedBrowserLocale(const std::string& new_locale)
      : old_locale_(g_browser_process->GetApplicationLocale()) {
  g_browser_process->SetApplicationLocale(new_locale);
}

ScopedBrowserLocale::~ScopedBrowserLocale() {
  g_browser_process->SetApplicationLocale(old_locale_);
}
