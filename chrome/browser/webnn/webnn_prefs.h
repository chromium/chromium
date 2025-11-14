// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBNN_WEBNN_PREFS_H_
#define CHROME_BROWSER_WEBNN_WEBNN_PREFS_H_

class PrefRegistrySimple;

namespace webnn {

namespace prefs {

inline constexpr char kWinAppRuntimePackageFamilyName[] =
    "win_app_runtime_package.family_name";
inline constexpr char kWinAppRuntimePackageMinVersion[] =
    "win_app_runtime_package.min_version";
inline constexpr char kWinAppRuntimePackageDependencyId[] =
    "win_app_runtime_package.dependency_id";

}  // namespace prefs

void RegisterLocalPrefs(PrefRegistrySimple* registry);

}  // namespace webnn

#endif  // CHROME_BROWSER_WEBNN_WEBNN_PREFS_H_
