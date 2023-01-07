// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_METRICS_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_METRICS_H_

#include "chrome/common/extensions/extension_constants.h"
#include "extensions/common/manifest.h"

namespace extensions {

class Extension;

// Records the given type of app launch for UMA.
void RecordAppLaunchType(extension_misc::AppLaunchBucket bucket,
                         extensions::Manifest::Type app_type);

// Records an app launch from the search view of the app list.
void RecordAppListSearchLaunch(const extensions::Extension* extension);

// Records an app launch from the main view of the app list.
void RecordAppListMainLaunch(const extensions::Extension* extension);

// Records a web store launch in the appropriate histograms.
void RecordWebStoreLaunch();

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_METRICS_H_
