// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_SYSTEM_WEB_APP_TYPES_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_SYSTEM_WEB_APP_TYPES_H_

namespace web_app {

// An enum that lists the different System Apps that exist. Can be used to
// retrieve the App ID from the underlying Web App system.
//
// These values are persisted to the web_app database. Entries should not be
// renumbered and numeric values should never be reused.
//
// When adding a new type, please add to the end, use an explicit number, and
// add a corresponding value to the proto enum in web_app.proto.
//
// When deprecating, comment out the entry so that it's not accidently re-used.
enum class SystemAppType {
  FILE_MANAGER = 1,
  TELEMETRY = 2,
  SAMPLE = 3,
  SETTINGS = 4,
  CAMERA = 5,
  TERMINAL = 6,
  MEDIA = 7,
  HELP = 8,
  PRINT_MANAGEMENT = 9,
  SCANNING = 10,
  DIAGNOSTICS = 11,
  CONNECTIVITY_DIAGNOSTICS = 12,

  // When adding a new System App, add a corresponding histogram suffix in
  // WebAppSystemAppInternalName (histograms.xml). The suffix name should match
  // the App's |internal_name|. This is for reporting per-app install results.

};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_SYSTEM_WEB_APP_TYPES_H_
