// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_BROWSER_PLATFORM_SPELL_CHECKER_H_
#define COMPONENTS_SPELLCHECK_BROWSER_PLATFORM_SPELL_CHECKER_H_

#include "components/spellcheck/browser/spellcheck_platform.h"

// An interface that lets SpellcheckService own an instance of a platform-
// specific spell check object. Platform-specific spell check objects are
// meant to interact directly with native APIs.
//
// This is needed for platforms where platform-specific spell check objects must
// hold a state based on the user's profile, such as on Windows where
// WindowsSpellChecker holds native COM objects for each of the profile's spell
// check languages.
//
// For platforms where a per-profile, platform-specific spell check object is
// not needed, like on Mac and Android, this interface doesn't need to be
// implemented, since the native APIs will be invoked directly.
//
// In summary:
// - Each platform has its own native APIs
// - The functions inside the spellcheck_platform:: namespace are responsible
//     for knowing how to interact with the native APIs, either directly or via
//     a platform-specific spell check object
// - If a platform-specific spell check object is required, then its instance is
//     owned by SpellcheckService, and a pointer to it is passed to each
//     function of the spellcheck_platform:: namespace
// - If the platform doesn't need a per-profile, platform-specific spell check
//     object, then the pointer passed to the functions in the
//     spellcheck_platform:: namespace is null and the functions can ignore it
//     (since they use other means to invoke native APIs)
class PlatformSpellChecker {
 public:
  virtual ~PlatformSpellChecker() = default;

  virtual void RequestTextCheck(
      int document_tag,
      const std::u16string& text,
      spellcheck_platform::TextCheckCompleteCallback callback) = 0;
};

#endif  // COMPONENTS_SPELLCHECK_BROWSER_PLATFORM_SPELL_CHECKER_H_
