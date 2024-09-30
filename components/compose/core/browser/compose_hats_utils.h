// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_HATS_UTILS_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_HATS_UTILS_H_

namespace compose::hats {

struct HatsFields {
  // Hats String data fields:

  static constexpr char kSessionID[] =
      "Execution ID linked to your recent input and page context";

  static constexpr char kURL[] = "Url";
  static constexpr char kLocale[] = "Locale";
  // Hats  Bits data fields:

  static constexpr char kResponseModified[] =
      "Session used a modifier, like elaborate or formal";
  static constexpr char kSessionContainedFilteredResponse[] =
      "A safety filter edited a response in this session";
  static constexpr char kSessionContainedError[] =
      "Any error appeared in this session";
  static constexpr char kSessionBeganWithNudge[] =
      "This session started with nudge";
};

}  // namespace compose::hats

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_HATS_UTILS_H_
