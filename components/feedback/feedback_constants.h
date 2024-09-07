// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_FEEDBACK_CONSTANTS_H_
#define COMPONENTS_FEEDBACK_FEEDBACK_CONSTANTS_H_

#include <string>

namespace feedback {

inline constexpr std::string_view kMahiMetadataKey = "from_mahi";
inline constexpr std::string_view kSeaPenMetadataKey = "from_sea_pen";
inline constexpr std::string_view kConchMetadataKey = "from_conch";

#if BUILDFLAG(IS_CHROMEOS_ASH)
inline constexpr int kChromeOSProductId = 208;
#endif
inline constexpr int kChromeBrowserProductId = 237;
inline constexpr int kOrcaFeedbackProductId = 5314436;
inline constexpr int kMahiFeedbackProductId = 5329991;
inline constexpr int kLobsterFeedbackProductId = 5342213;

}  // namespace feedback

#endif  // COMPONENTS_FEEDBACK_FEEDBACK_CONSTANTS_H_
