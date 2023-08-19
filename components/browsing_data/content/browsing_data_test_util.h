// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_TEST_UTIL_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_TEST_UTIL_H_

#include <string>

namespace content {
class WebContents;
class RenderFrameHost;
}

namespace browsing_data_test_util {

bool HasDataForType(const std::string& type,
                    content::WebContents* web_contents);

void SetDataForType(const std::string& type,
                    content::WebContents* web_contents);

void SetDataForType(const std::string& type,
                    content::RenderFrameHost* render_frame_host);

}  // namespace browsing_data_test_util

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_TEST_UTIL_H_
