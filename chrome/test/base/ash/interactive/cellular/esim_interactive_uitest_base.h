// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_INTERACTIVE_CELLULAR_ESIM_INTERACTIVE_UITEST_BASE_H_
#define CHROME_TEST_BASE_ASH_INTERACTIVE_CELLULAR_ESIM_INTERACTIVE_UITEST_BASE_H_

#include "chrome/test/base/ash/interactive/cellular/cellular_util.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"

namespace ash {

// Base test of the eSIM interactive UI tests. `SetUpOnMainThread` will add
// an EUICC with a carrier profile and connect to the eSIM service.
class EsimInteractiveUiTestBase : public InteractiveAshTest {
 protected:
  // InteractiveAshTest:
  void SetUpOnMainThread() override;

  // Disconnect the active esim service.
  void DisconnectEsimService();

  const SimInfo& esim_info() { return esim_info_; }
  const EuiccInfo& euicc_info() { return euicc_info_; }

 private:
  const EuiccInfo euicc_info_ = EuiccInfo{/*id=*/0};
  const SimInfo esim_info_ = SimInfo{/*id=*/0};
};

}  // namespace ash

#endif  // CHROME_TEST_BASE_ASH_INTERACTIVE_CELLULAR_ESIM_INTERACTIVE_UITEST_BASE_H_
