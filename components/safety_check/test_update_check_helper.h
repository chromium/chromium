// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFETY_CHECK_TEST_UPDATE_CHECK_HELPER_H_
#define COMPONENTS_SAFETY_CHECK_TEST_UPDATE_CHECK_HELPER_H_

#include "components/safety_check/update_check_helper.h"

namespace safety_check {

// A version of UpdateCheckHelper used for testing.
class TestUpdateCheckHelper : public UpdateCheckHelper {
 public:
  TestUpdateCheckHelper();
  ~TestUpdateCheckHelper() override;

  void SetConnectivity(bool online);

  // UpdateCheckHelper implementation.
  void CheckConnectivity(
      ConnectivityCheckCallback connection_check_callback) override;

 private:
  bool online_ = true;
};

}  // namespace safety_check

#endif  // COMPONENTS_SAFETY_CHECK_TEST_UPDATE_CHECK_HELPER_H_
