// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_test_util.h"

#include "ash/birch/birch_item_remover.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/overview/birch/birch_chip_button_base.h"
#include "ash/wm/overview/overview_grid_test_api.h"
#include "base/run_loop.h"
#include "components/prefs/pref_service.h"

namespace ash {

void EnsureItemRemoverInitialized() {
  BirchItemRemover* remover =
      Shell::Get()->birch_model()->GetItemRemoverForTest();
  if (!remover->Initialized()) {
    base::RunLoop run_loop;
    remover->SetProtoInitCallbackForTest(run_loop.QuitClosure());
    run_loop.Run();
  }
}

BirchChipButtonBase* GetBirchChipButton() {
  aura::Window* root = Shell::GetPrimaryRootWindow();
  OverviewGridTestApi test_api(root);
  CHECK(test_api.birch_bar_view());
  CHECK_EQ(test_api.GetBirchChips().size(), 1u);
  return test_api.GetBirchChips()[0];
}

void DisableAllDataTypePrefsExcept(std::vector<std::string_view> exceptions) {
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  CHECK(pref_service);
  const std::string_view kDataPrefs[] = {
      prefs::kBirchUseCalendar,     prefs::kBirchUseFileSuggest,
      prefs::kBirchUseChromeTabs,   prefs::kBirchUseLostMedia,
      prefs::kBirchUseReleaseNotes, prefs::kBirchUseWeather,
      prefs::kBirchUseCoral,
  };
  for (const std::string_view pref : kDataPrefs) {
    const bool enable = base::Contains(exceptions, pref);
    pref_service->SetBoolean(pref, enable);
  }
}

}  // namespace ash
