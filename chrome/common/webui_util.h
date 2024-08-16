// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_WEBUI_UTIL_H_
#define CHROME_COMMON_WEBUI_UTIL_H_

class GURL;

namespace chrome {

// Returns true if the generated code cache should be used for a given resource
// `request_url`.
bool ShouldUseCodeCacheForWebUIUrl(const GURL& request_url);

}  // namespace chrome

#endif  // CHROME_COMMON_WEBUI_UTIL_H_
