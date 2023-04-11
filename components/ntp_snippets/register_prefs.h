// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REGISTER_PREFS_H_
#define COMPONENTS_NTP_SNIPPETS_REGISTER_PREFS_H_

class PrefRegistrySimple;
class PrefService;

namespace ntp_snippets::prefs {

void RegisterProfilePrefsForMigrationApril2023(PrefRegistrySimple* registry);
void MigrateObsoleteProfilePrefsApril2023(PrefService* prefs);

}  // namespace ntp_snippets::prefs

#endif  // COMPONENTS_NTP_SNIPPETS_REGISTER_PREFS_H_
