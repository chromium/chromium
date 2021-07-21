// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_BLUETOOTH_SHARED_LOAD_TIME_DATA_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_BLUETOOTH_SHARED_LOAD_TIME_DATA_PROVIDER_H_

namespace content {
class WebUIDataSource;
}  // namespace content

namespace chromeos {
namespace bluetooth {

// Adds the data needed for bluetooth elements to |html_source|.
void AddLoadTimeData(content::WebUIDataSource* html_source);

}  // namespace bluetooth
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_BLUETOOTH_SHARED_LOAD_TIME_DATA_PROVIDER_H_
