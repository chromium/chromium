// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "components/app_constants/constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

namespace extensions {

void RecordAppLaunchType(extension_misc::AppLaunchBucket bucket,
                         extensions::Manifest::Type app_type) {
  DCHECK_LT(bucket, extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);
  if (app_type == extensions::Manifest::TYPE_PLATFORM_APP) {
    UMA_HISTOGRAM_ENUMERATION("Apps.AppLaunch", bucket,
                              extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Extensions.AppLaunch", bucket,
                              extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);
  }
}

void RecordAppListSearchLaunch(const extensions::Extension* extension) {
  extension_misc::AppLaunchBucket bucket =
      extension_misc::APP_LAUNCH_APP_LIST_SEARCH;
  if (extension->id() == extensions::kWebStoreAppId)
    bucket = extension_misc::APP_LAUNCH_APP_LIST_SEARCH_WEBSTORE;
  else if (extension->id() == app_constants::kChromeAppId)
    bucket = extension_misc::APP_LAUNCH_APP_LIST_SEARCH_CHROME;
  RecordAppLaunchType(bucket, extension->GetType());
}

void RecordAppListMainLaunch(const extensions::Extension* extension) {
  extension_misc::AppLaunchBucket bucket =
      extension_misc::APP_LAUNCH_APP_LIST_MAIN;
  if (extension->id() == extensions::kWebStoreAppId)
    bucket = extension_misc::APP_LAUNCH_APP_LIST_MAIN_WEBSTORE;
  else if (extension->id() == app_constants::kChromeAppId)
    bucket = extension_misc::APP_LAUNCH_APP_LIST_MAIN_CHROME;
  RecordAppLaunchType(bucket, extension->GetType());
}

void RecordWebStoreLaunch() {
  RecordAppLaunchType(extension_misc::APP_LAUNCH_NTP_WEBSTORE,
                      extensions::Manifest::TYPE_HOSTED_APP);
}

}  // namespace extensions
