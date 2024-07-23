// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_INTERACTIVE_CELLULAR_ESIM_INTERACTIVE_UITEST_BASE_H_
#define CHROME_TEST_BASE_ASH_INTERACTIVE_CELLULAR_ESIM_INTERACTIVE_UITEST_BASE_H_

#include "chrome/test/base/ash/interactive/cellular/cellular_util.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"

namespace ash {

// Base test class for eSIM interactive UI tests. `SetUpOnMainThread` will set
// up the context, install system apps, clear existing eUICCs, and add an eUICC.
class EsimInteractiveUiTestBase : public InteractiveAshTest {
 protected:
  // InteractiveAshTest:
  void SetUpOnMainThread() override;

  const EuiccInfo& euicc_info() { return euicc_info_; }

 private:
  const EuiccInfo euicc_info_{/*id=*/0};
};

}  // namespace ash

#endif  // CHROME_TEST_BASE_ASH_INTERACTIVE_CELLULAR_ESIM_INTERACTIVE_UITEST_BASE_H_
