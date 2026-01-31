// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MAGIC_BOOST_TEST_MOCK_MAGIC_BOOST_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_MAGIC_BOOST_TEST_MOCK_MAGIC_BOOST_CONTROLLER_H_

#include "chrome/browser/ash/magic_boost/magic_boost_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockMagicBoostController : public ash::MagicBoostController {
 public:
  MockMagicBoostController();
  ~MockMagicBoostController() override;

  MOCK_METHOD(void,
              ShowDisclaimerUi,
              (int64_t,
               ash::magic_boost::TransitionAction,
               ash::magic_boost::OptInFeatures),
              (override));
  MOCK_METHOD(void, CloseDisclaimerUi, (), (override));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_ASH_MAGIC_BOOST_TEST_MOCK_MAGIC_BOOST_CONTROLLER_H_
