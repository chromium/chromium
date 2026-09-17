// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VERSION_INFO_VERSION_INFO_WITH_USER_AGENT_H_
#define COMPONENTS_VERSION_INFO_VERSION_INFO_WITH_USER_AGENT_H_

#include <string>

namespace version_info {

// Returns the product name and reduced version information for the User-Agent
// header, in the format: Chrome/<major_version>.0.0.0
std::string GetProductNameAndVersionForReducedUserAgent();

// Returns the product name and version information for the User-Agent header,
// in the format: Chrome/<major_version>.<minor_version>.<build>.<patch>.
std::string GetProductNameAndVersionForUserAgent();

}  // namespace version_info

#endif  // COMPONENTS_VERSION_INFO_VERSION_INFO_WITH_USER_AGENT_H_
