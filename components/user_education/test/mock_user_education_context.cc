// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/test/mock_user_education_context.h"

#include "ui/base/interaction/framework_specific_implementation.h"

namespace user_education::test {

DEFINE_FRAMEWORK_SPECIFIC_METADATA(MockUserEducationContext)

MockUserEducationContext::MockUserEducationContext() = default;
MockUserEducationContext::~MockUserEducationContext() = default;

}  // namespace user_education::test
