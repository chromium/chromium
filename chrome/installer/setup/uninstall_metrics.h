// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_UNINSTALL_METRICS_H_
#define CHROME_INSTALLER_SETUP_UNINSTALL_METRICS_H_

#include "base/strings/string16.h"

namespace base {
class FilePath;
class Value;
}  // namespace base

namespace installer {

// Extracts uninstall metrics from the given JSON value.
bool ExtractUninstallMetrics(const base::Value& root,
                             base::string16* uninstall_metrics);

// Extracts uninstall metrics from the JSON file located at file_path.
// Returns them in a form suitable for appending to a url that already
// has GET parameters, i.e. &metric1=foo&metric2=bar.
// Returns true if uninstall_metrics has been successfully populated with
// the uninstall metrics, false otherwise.
bool ExtractUninstallMetricsFromFile(const base::FilePath& file_path,
                                     base::string16* uninstall_metrics);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_UNINSTALL_METRICS_H_
