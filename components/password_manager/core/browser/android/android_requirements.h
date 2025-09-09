// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_ANDROID_REQUIREMENTS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_ANDROID_REQUIREMENTS_H_

namespace password_manager {

// See AndroidRequirements.isPasswordManagerAvailable() in Java.
bool IsPasswordManagerAvailable();

// See AndroidRequirements.hasMinGmsVersion() in Java.
bool HasMinGmsVersion();

// See AndroidRequirements.hasInternalBackend() in Java.
bool HasInternalBackend();

void SetAndroidRequirementsForTesting(bool has_min_gms_version,
                                      bool has_internal_backend);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_ANDROID_REQUIREMENTS_H_
