// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CONTENT_BROWSER_LANGUAGE_CODE_LOCATOR_PROVIDER_H_
#define COMPONENTS_LANGUAGE_CONTENT_BROWSER_LANGUAGE_CODE_LOCATOR_PROVIDER_H_

#include <memory>

class PrefService;

namespace language {

class LanguageCodeLocator;

std::unique_ptr<LanguageCodeLocator> GetLanguageCodeLocator(PrefService* prefs);

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CONTENT_BROWSER_LANGUAGE_CODE_LOCATOR_PROVIDER_H_
