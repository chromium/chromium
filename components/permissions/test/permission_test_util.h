// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_TEST_PERMISSION_TEST_UTIL_H_
#define COMPONENTS_PERMISSIONS_TEST_PERMISSION_TEST_UTIL_H_

#include <memory>

namespace content {
class PermissionControllerDelegate;
class BrowserContext;
}  // namespace content

namespace permissions {

std::unique_ptr<content::PermissionControllerDelegate>
GetPermissionControllerDelegate(content::BrowserContext* context);

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_TEST_PERMISSION_TEST_UTIL_H_
