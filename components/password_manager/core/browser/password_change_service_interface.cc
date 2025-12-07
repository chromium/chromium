// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_change_service_interface.h"

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "components/password_manager/core/browser/password_manager_switches.h"

namespace password_manager {

std::vector<GURL> GetChangePasswordUrlOverrides() {
  std::string urls =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kPasswordChangeUrl);
  std::vector<GURL> result;
  for (const auto& cur : base::SplitStringPiece(
           urls, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    result.emplace_back(cur);
  }
  return result;
}

}  // namespace password_manager
