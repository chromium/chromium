// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_WEB_COMPONENTS_PREFS_H_
#define CHROME_COMMON_WEB_COMPONENTS_PREFS_H_

class PrefRegistrySimple;

namespace web_components_prefs {

// Register preferences for Web Components.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace web_components_prefs

#endif  // CHROME_COMMON_WEB_COMPONENTS_PREFS_H_
