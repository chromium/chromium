// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_SCOPED_TEST_SYSTEM_NSS_KEY_SLOT_MIXIN_H_
#define CHROME_TEST_BASE_ASH_SCOPED_TEST_SYSTEM_NSS_KEY_SLOT_MIXIN_H_

#include <pk11pub.h>

#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "crypto/scoped_nss_types.h"

namespace ash {

// Owns a persistent NSS software database in the user directory and the
// association of the system slot with this database.
// Note: The database is persisted in the user data directory
// (chrome::DIR_USER_DATA) so it persists between PRE_ and non-PRE_ tests. This
// allows simulating browser restarts after doing some operations on the
// database without losing its state.
//
// This mixin performs the blocking initialization/destruction in the
// SetUp*|TearDown* methods.
class ScopedTestSystemNSSKeySlotMixin final : public InProcessBrowserTestMixin {
 public:
  explicit ScopedTestSystemNSSKeySlotMixin(InProcessBrowserTestMixinHost* host);
  ScopedTestSystemNSSKeySlotMixin(const ScopedTestSystemNSSKeySlotMixin&) =
      delete;
  ScopedTestSystemNSSKeySlotMixin& operator=(
      const ScopedTestSystemNSSKeySlotMixin&) = delete;
  ~ScopedTestSystemNSSKeySlotMixin() override;

  PK11SlotInfo* slot() { return slot_.get(); }

  // SetUp and TearDown are not symmetrical. SetUp has to happen very early in a
  // test life cycle (before ChromeOSTokenManager is created). And TearDown
  // should happen while the IO thread still exists to properly close the
  // database.
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownOnMainThread() override;

 private:
  void DestroyOnIo();

  crypto::ScopedPK11Slot slot_;
};

}  // namespace ash

#endif  // CHROME_TEST_BASE_ASH_SCOPED_TEST_SYSTEM_NSS_KEY_SLOT_MIXIN_H_
