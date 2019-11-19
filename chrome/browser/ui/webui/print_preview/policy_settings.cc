// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/policy_settings.h"

#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace printing {

// static
void PolicySettings::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kPrinterTypeBlacklist);
  registry->RegisterBooleanPref(prefs::kPrintHeaderFooter, true);
#if defined(OS_CHROMEOS)
  registry->RegisterIntegerPref(prefs::kPrintingAllowedBackgroundGraphicsModes,
                                0);
  registry->RegisterIntegerPref(prefs::kPrintingAllowedColorModes, 0);
  registry->RegisterIntegerPref(prefs::kPrintingAllowedDuplexModes, 0);
  registry->RegisterIntegerPref(prefs::kPrintingAllowedPinModes, 0);
  registry->RegisterListPref(prefs::kPrintingAllowedPageSizes);
  registry->RegisterIntegerPref(prefs::kPrintingBackgroundGraphicsDefault, 0);
  registry->RegisterIntegerPref(prefs::kPrintingColorDefault, 0);
  registry->RegisterIntegerPref(prefs::kPrintingDuplexDefault, 0);
  registry->RegisterIntegerPref(prefs::kPrintingPinDefault, 0);
  registry->RegisterDictionaryPref(prefs::kPrintingSizeDefault);
#endif
}

}  // namespace printing
