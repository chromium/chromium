// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_SYSTEM_ERROR_PAGE_ERROR_PAGE_POPULATOR_H_
#define COMPONENTS_ANDROID_SYSTEM_ERROR_PAGE_ERROR_PAGE_POPULATOR_H_

#include <string>

namespace blink {
struct WebURLError;
}

namespace android_system_error_page {

// Populates |error_html| to display an error page with an Android system feel
// for |error|. Does nothing if |error_html| is null.
void PopulateErrorPageHtml(const blink::WebURLError& error,
                           std::string* error_html);

}  // namespace android_system_error_page

#endif  // COMPONENTS_ANDROID_SYSTEM_ERROR_PAGE_ERROR_PAGE_POPULATOR_H_
