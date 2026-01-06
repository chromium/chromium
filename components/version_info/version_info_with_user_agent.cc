// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/version_info/version_info_with_user_agent.h"

#include "base/strings/strcat.h"
#include "base/version_info/version_info.h"

namespace version_info {

std::string GetProductNameAndVersionForReducedUserAgent() {
  return base::StrCat({"Chrome/", GetMajorVersionNumber(), ".0.0.0"});
}

}  // namespace version_info
