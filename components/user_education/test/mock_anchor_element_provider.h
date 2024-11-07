// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_TEST_MOCK_ANCHOR_ELEMENT_PROVIDER_H_
#define COMPONENTS_USER_EDUCATION_TEST_MOCK_ANCHOR_ELEMENT_PROVIDER_H_

#include "components/user_education/common/anchor_element_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace user_education::test {

// Mocks the `AnchorElementProvider` interface.
class MockAnchorElementProvider : public AnchorElementProvider {
 public:
  MockAnchorElementProvider();
  ~MockAnchorElementProvider() override;

  // AnchorElementProvider:
  MOCK_METHOD(ui::TrackedElement*,
              GetAnchorElement,
              (ui::ElementContext),
              (const, override));
};

}  // namespace user_education::test

#endif  // COMPONENTS_USER_EDUCATION_TEST_MOCK_ANCHOR_ELEMENT_PROVIDER_H_
