// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_LOCALE_UTIL_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_LOCALE_UTIL_H_

#include <string>

class PrefService;

namespace language {

// Returns the current application locale (e.g. "en-US").
std::string GetApplicationLocale(PrefService* local_state);

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_LOCALE_UTIL_H_
