// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/policy_settings.h"

#include "build/chromeos_buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace printing {

// static
void PolicySettings::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kPrinterTypeDenyList);
  registry->RegisterBooleanPref(prefs::kPrintHeaderFooter, true);
  registry->RegisterIntegerPref(prefs::kPrintingAllowedBackgroundGraphicsModes,
                                0);
  registry->RegisterIntegerPref(prefs::kPrintingBackgroundGraphicsDefault, 0);
  registry->RegisterDictionaryPref(prefs::kPrintingPaperSizeDefault);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterIntegerPref(prefs::kPrintingAllowedColorModes, 0);
  registry->RegisterIntegerPref(prefs::kPrintingAllowedDuplexModes, 0);
  registry->RegisterIntegerPref(prefs::kPrintingAllowedPinModes, 0);
  registry->RegisterIntegerPref(prefs::kPrintingColorDefault, 0);
  registry->RegisterIntegerPref(prefs::kPrintingDuplexDefault, 0);
  registry->RegisterIntegerPref(prefs::kPrintingPinDefault, 0);
  registry->RegisterIntegerPref(prefs::kPrintingMaxSheetsAllowed, -1);
#endif
}

}  // namespace printing
