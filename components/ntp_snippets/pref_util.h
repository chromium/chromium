// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_PREF_UTIL_H_
#define COMPONENTS_NTP_SNIPPETS_PREF_UTIL_H_

#include <set>
#include <string>

class PrefService;

namespace ntp_snippets {
namespace prefs {

// Reads a given preference and then deserializes it into a set of strings.
std::set<std::string> ReadDismissedIDsFromPrefs(const PrefService& pref_service,
                                                const std::string& pref_name);

// Serializes a set of strings into a given preference.
void StoreDismissedIDsToPrefs(PrefService* pref_service,
                              const std::string& pref_name,
                              const std::set<std::string>& dismissed_ids);

}  // namespace prefs
}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_PREF_UTIL_H_
