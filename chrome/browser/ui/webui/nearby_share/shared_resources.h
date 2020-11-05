// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEARBY_SHARE_SHARED_RESOURCES_H_
#define CHROME_BROWSER_UI_WEBUI_NEARBY_SHARE_SHARED_RESOURCES_H_

#include "content/public/browser/web_ui_data_source.h"

extern const char kNearbyShareGeneratedPath[];

void RegisterNearbySharedMojoResources(content::WebUIDataSource* data_source);
void RegisterNearbySharedResources(content::WebUIDataSource* data_source);
void RegisterNearbySharedStrings(content::WebUIDataSource* data_source);

#endif  // CHROME_BROWSER_UI_WEBUI_NEARBY_SHARE_SHARED_RESOURCES_H_
