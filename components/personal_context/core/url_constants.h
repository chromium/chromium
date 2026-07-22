// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_URL_CONSTANTS_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_URL_CONSTANTS_H_

namespace personal_context {

// This URL should be used when a user has not opted in to personal
// intelligence and leads to a comprehensive PI Help Center page.
inline constexpr char kPersonalContextLearnMoreURL[] =
    "https://support.google.com/chrome?p=chrome_pi";

// This URL should be used when a user has already opted in to personal
// intelligence but may want to learn more about associated features.
inline constexpr char kPersonalContextOptedInLearnMoreURL[] =
    "https://support.google.com/chrome?p=chrome_pi_recall";

inline constexpr char kPersonalContextTriggerText[] = "@@";

inline constexpr char kPersonalContextConnectedAppsURL[] =
    "https://gemini.google.com/apps";

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_URL_CONSTANTS_H_
