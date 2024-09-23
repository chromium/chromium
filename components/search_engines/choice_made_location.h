// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_CHOICE_MADE_LOCATION_H_
#define COMPONENTS_SEARCH_ENGINES_CHOICE_MADE_LOCATION_H_

namespace search_engines {

// The location from which the default search engine was set.
//
// Maintained by chrome-waffle-eng@google.com, please reach out if looking into
// adding some new entry here, as there are some requirements that non-`kOther`
// entries must meet.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.search_engines
// LINT.IfChange
enum class ChoiceMadeLocation {
  // `chrome://settings/search`
  kSearchSettings = 0,
  // `chrome://settings/searchEngines`
  // This value is also used for the settings pages on mobile.
  kSearchEngineSettings = 1,
  // The search engine choice dialog for existing users or the profile picker
  // for new users.
  kChoiceScreen = 2,
  // Some other source, not matching some requirements that the full search
  // engine choice surfaces are compatible with. Might be used for example when
  // automatically changing default search engine via an extension, or some
  // enterprise policy.
  kOther = 3,
  kMaxValue = kOther,
};
// LINT.ThenChange(chrome/browser/resources/settings/search_engines_page/search_engines_browser_proxy.ts)

}  // namespace search_engines

#endif  // COMPONENTS_SEARCH_ENGINES_CHOICE_MADE_LOCATION_H_
