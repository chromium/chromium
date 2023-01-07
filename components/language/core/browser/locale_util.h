// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_LOCALE_UTIL_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_LOCALE_UTIL_H_

#include <string>

class PrefService;

namespace language {

// This method is responsible for determining the initial locale based on
// command line flags, preferences, and OS settings. In nearly all cases you
// shouldn't call this, rather use GetApplicationLocale defined on browser
// process.
// Returns the current application locale (e.g. "en-US").
std::string GetApplicationLocale(PrefService* local_state);

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_LOCALE_UTIL_H_
