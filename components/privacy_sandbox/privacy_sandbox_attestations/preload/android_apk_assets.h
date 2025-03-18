// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRELOAD_ANDROID_APK_ASSETS_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRELOAD_ANDROID_APK_ASSETS_H_

#include <string_view>

namespace privacy_sandbox {

inline constexpr std::string_view kManifestAssetPath =
    "assets/privacy_sandbox_attestations/manifest.json";

inline constexpr std::string_view kAttestationsListAssetPath =
    "assets/privacy_sandbox_attestations/privacy-sandbox-attestations.dat";

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRELOAD_ANDROID_APK_ASSETS_H_
