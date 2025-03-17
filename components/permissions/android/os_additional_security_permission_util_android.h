// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_OS_ADDITIONAL_SECURITY_PERMISSION_UTIL_ANDROID_H_
#define COMPONENTS_PERMISSIONS_ANDROID_OS_ADDITIONAL_SECURITY_PERMISSION_UTIL_ANDROID_H_

namespace permissions {
// Returns whether the operating system has granted permission to enable
// Javascript optimizers. Can be queried from any thread.
bool HasJavascriptOptimizerPermission();
}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_OS_ADDITIONAL_SECURITY_PERMISSION_UTIL_ANDROID_H_
