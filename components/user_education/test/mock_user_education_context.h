// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_TEST_MOCK_USER_EDUCATION_CONTEXT_H_
#define COMPONENTS_USER_EDUCATION_TEST_MOCK_USER_EDUCATION_CONTEXT_H_

#include "components/user_education/common/user_education_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/framework_specific_implementation.h"

namespace user_education::test {

class MockUserEducationContext : public UserEducationContext {
 public:
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  MockUserEducationContext();

  MOCK_METHOD(bool, IsValid, (), (const, override));
  MOCK_METHOD(ui::ElementContext, GetElementContext, (), (const, override));
  MOCK_METHOD(const ui::AcceleratorProvider*,
              GetAcceleratorProvider,
              (),
              (const, override));

 protected:
  ~MockUserEducationContext() override;
};

}  // namespace user_education::test

#endif  // COMPONENTS_USER_EDUCATION_TEST_MOCK_USER_EDUCATION_CONTEXT_H_
