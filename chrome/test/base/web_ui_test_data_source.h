// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_WEB_UI_TEST_DATA_SOURCE_H_
#define CHROME_TEST_BASE_WEB_UI_TEST_DATA_SOURCE_H_

#include "content/public/browser/web_ui_data_source.h"

namespace webui {

// Creates a data source for for chrome://webui-test/ URLs.
content::WebUIDataSource* CreateWebUITestDataSource();

}  // namespace webui

#endif  // CHROME_TEST_BASE_WEB_UI_TEST_DATA_SOURCE_H_
