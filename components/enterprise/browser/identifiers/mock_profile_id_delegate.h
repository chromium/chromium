// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_IDENTIFIERS_MOCK_PROFILE_ID_DELEGATE_H_
#define COMPONENTS_ENTERPRISE_BROWSER_IDENTIFIERS_MOCK_PROFILE_ID_DELEGATE_H_

#include "components/enterprise/browser/identifiers/profile_id_delegate.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise::test {

// Mocked implementation of the ProfileIdDelegate interface.
class MockProfileIdDelegate : public ProfileIdDelegate {
 public:
  MockProfileIdDelegate();
  ~MockProfileIdDelegate() override;

  MOCK_METHOD(std::string, GetDeviceId, (), (override));
};

}  // namespace enterprise::test

#endif  // COMPONENTS_ENTERPRISE_BROWSER_IDENTIFIERS_MOCK_PROFILE_ID_DELEGATE_H_
