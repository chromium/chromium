// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_SHARE_TARGET_TARGET_UTIL_H_
#define CHROME_BROWSER_WEB_SHARE_TARGET_TARGET_UTIL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"

namespace network {
class ResourceRequestBody;
}

namespace web_share_target {

// Return a string as a quoted value, escaping quotes and line breaks.
std::string PercentEscapeString(const std::string& unescaped_string);

// Compute and return multipart/form-data POST body for share target.
scoped_refptr<network::ResourceRequestBody> ComputeMultipartBody(
    const std::vector<std::string>& names,
    const std::vector<std::string>& values,
    const std::vector<bool>& is_value_file_uris,
    const std::vector<std::string>& filenames,
    const std::vector<std::string>& types,
    const std::string& boundary);

// Compute and return application/x-www-form-urlencoded POST body for share
// target.
std::string ComputeUrlEncodedBody(const std::vector<std::string>& names,
                                  const std::vector<std::string>& values);

}  // namespace web_share_target

#endif  // CHROME_BROWSER_WEB_SHARE_TARGET_TARGET_UTIL_H_
