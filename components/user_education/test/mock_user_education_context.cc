// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/test/mock_user_education_context.h"

#include "ui/base/interaction/safe_castable.h"

namespace user_education::test {

DEFINE_SAFE_CAST_TARGET(MockUserEducationContext)

MockUserEducationContext::MockUserEducationContext() = default;
MockUserEducationContext::~MockUserEducationContext() = default;

}  // namespace user_education::test
