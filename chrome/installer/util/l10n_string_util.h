// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains helper functions for getting strings that are included in
// our DLL for all languages (i.e., does not come from our language DLL).
//
// These resource strings are organized such that we can get a localized string
// by taking the base resource ID and adding a language offset.  For example,
// to get the resource id for the localized product name in en-US, we take
// IDS_PRODUCT_NAME_BASE + IDS_L10N_OFFSET_EN_US.

#ifndef CHROME_INSTALLER_UTIL_L10N_STRING_UTIL_H_
#define CHROME_INSTALLER_UTIL_L10N_STRING_UTIL_H_

#include <string>
#include <vector>

namespace installer {

class TranslationDelegate {
 public:
  virtual ~TranslationDelegate();
  virtual std::wstring GetLocalizedString(int installer_string_id) = 0;
};

// If we're in Chrome, the installer strings aren't in the binary, but are in
// the localized pak files.  A TranslationDelegate must be provided so we can
// load these strings.
void SetTranslationDelegate(TranslationDelegate* delegate);

// Given a string base id, return the localized version of the string based on
// the system language. This is used for shortcuts placed on the user's desktop.
// The string is retrieved from the TranslationDelegate if one has been set.
// Otherwise, the string is read from the binary's string table. Certain
// messages (see MODE_SPECIFIC_STRINGS in create_string_rc.py) are dynamically
// mapped to a variant that is specific to the current install mode (e.g.,
// IDS_INBOUND_MDNS_RULE_NAME is mapped to IDS_INBOUND_MDNS_RULE_NAME_CANARY for
// canary Chrome).
std::wstring GetLocalizedString(int base_message_id);

// Returns a formatted version of the localized string using `replacements` for
// the placeholders within `base_message_id`.
std::wstring GetLocalizedStringF(int base_message_id,
                                 std::vector<std::wstring> replacements);

// Given the system language, return a url that points to the localized eula.
// The empty string is returned on failure.
std::wstring GetLocalizedEulaResource();

// Returns the language identifier of the translation currently in use.
std::wstring GetCurrentTranslation();

// Returns the mode-specific message id for |base_message_id| given the current
// install mode. Returns |base_message_id| itself if it does not have per-mode
// variants. See MODE_SPECIFIC_STRINGS in create_string_rc.py for details..
int GetBaseMessageIdForMode(int base_message_id);

}  // namespace installer.

#endif  // CHROME_INSTALLER_UTIL_L10N_STRING_UTIL_H_
