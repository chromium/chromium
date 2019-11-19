// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TEST_FILES_REQUEST_FILTER_H_
#define CHROME_BROWSER_UI_WEBUI_TEST_FILES_REQUEST_FILTER_H_

#include "content/public/browser/web_ui_data_source.h"

namespace test {

// Returns a callback to be used as a filter in WebUIDataSource.
// The filter responds with a content of "%DIR_TEST_DATA%/webui/<filename>" if
// request path has "/test/<filename>" format.
content::WebUIDataSource::HandleRequestCallback GetTestFilesRequestFilter();

// Returns a callback indicating which requests should be handled by the filter.
content::WebUIDataSource::ShouldHandleRequestCallback
GetTestShouldHandleRequest();

}  // namespace test

#endif  // CHROME_BROWSER_UI_WEBUI_TEST_FILES_REQUEST_FILTER_H_
