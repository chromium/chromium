// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_TEST_ENUMS_TO_STRING_H_
#define COMPONENTS_PERMISSIONS_TEST_ENUMS_TO_STRING_H_

#include "components/permissions/permission_request_enums.h"
#include "components/permissions/prediction_service/permission_ui_selector.h"
#include "components/permissions/request_type.h"

// Contains methods that convert permission relevant enums into strings. As
// there is no elegant C++ support for this, this provides methods to quickly
// convert enums into strings for testing purposes. This is for example helpful
// for parametrized test name generators or for simple logging.
namespace test {

std::string_view ToString(
    permissions::PermissionUiSelector::QuietUiReason ui_reason);

std::string_view ToString(permissions::RequestType request_type);

std::string_view ToString(
    permissions::PermissionRequestRelevance request_relevance);

}  // namespace test

#endif  // COMPONENTS_PERMISSIONS_TEST_ENUMS_TO_STRING_H_
