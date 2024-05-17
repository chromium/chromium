// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_TEST_MOCK_MAGIC_BOOST_CONTROLLER_H_
#define CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_TEST_MOCK_MAGIC_BOOST_CONTROLLER_H_

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

// A mock class for testing.
class MockMagicBoostController : public MagicBoostController {
 public:
  MockMagicBoostController();
  MockMagicBoostController(const MockMagicBoostController&) = delete;
  MockMagicBoostController& operator=(const MockMagicBoostController&) = delete;
  ~MockMagicBoostController();

  // chromeos::MahiManager:
  MOCK_METHOD(void, ShowOptInUi, (const gfx::Rect&), (override));
  MOCK_METHOD(void, CloseOptInUi, (), (override));
  MOCK_METHOD(bool, ShouldQuickAnswersAndMahiShowOptIn, (), (override));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_TEST_MOCK_MAGIC_BOOST_CONTROLLER_H_
