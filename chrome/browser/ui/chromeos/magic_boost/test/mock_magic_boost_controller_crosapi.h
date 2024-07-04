// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_TEST_MOCK_MAGIC_BOOST_CONTROLLER_CROSAPI_H_
#define CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_TEST_MOCK_MAGIC_BOOST_CONTROLLER_CROSAPI_H_

#include "chromeos/crosapi/mojom/magic_boost.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockMagicBoostControllerCrosapi
    : public crosapi::mojom::MagicBoostController {
 public:
  MockMagicBoostControllerCrosapi();
  ~MockMagicBoostControllerCrosapi() override;

  MOCK_METHOD(void,
              ShowDisclaimerUi,
              (int64_t,
               crosapi::mojom::MagicBoostController::TransitionAction,
               crosapi::mojom::MagicBoostController::OptInFeatures),
              (override));
  MOCK_METHOD(void, CloseDisclaimerUi, (), (override));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_TEST_MOCK_MAGIC_BOOST_CONTROLLER_CROSAPI_H_
