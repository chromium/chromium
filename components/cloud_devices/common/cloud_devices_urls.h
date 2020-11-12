// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CLOUD_DEVICES_COMMON_CLOUD_DEVICES_URLS_H_
#define COMPONENTS_CLOUD_DEVICES_COMMON_CLOUD_DEVICES_URLS_H_

#include <string>

#include "url/gurl.h"

namespace cloud_devices {

extern const char kCloudPrintAuthScope[];

GURL GetCloudPrintURL();
GURL GetCloudPrintRelativeURL(const std::string& relative_path);
GURL GetCloudPrintAddAccountURL();
GURL GetCloudPrintEnableURL(const std::string& proxy_id);
GURL GetCloudPrintEnableWithSigninURL(const std::string& proxy_id);
GURL GetCloudPrintManageDeviceURL(const std::string& device_id);
GURL GetCloudPrintPrintersURL();
GURL GetCloudPrintSigninURL();
bool IsCloudPrintURL(const GURL& url);

}  // namespace cloud_devices

#endif  // COMPONENTS_CLOUD_DEVICES_COMMON_CLOUD_DEVICES_URLS_H_
