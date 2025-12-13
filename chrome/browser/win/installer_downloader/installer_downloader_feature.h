// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_FEATURE_H_
#define CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_FEATURE_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace installer_downloader {

// When enabled, user may see the installer download UI flow.
BASE_DECLARE_FEATURE(kInstallerDownloader);

// Intentionally defaulted to empty string. It will be set by experiment.
inline constexpr base::FeatureParam<std::string> kLearnMoreUrl(
    &kInstallerDownloader,
    "learn_more_url",
    "https://support.google.com/chrome/?p=win10_transition");

// Intentionally defaulted to an empty string. It will either be set by an
// experiment or a default value will be computed during runtime.
inline constexpr base::FeatureParam<std::string> kInstallerUrlTemplateParam(
    &kInstallerDownloader,
    "installer_url_template",
    "");

// Indicates the file name of the downloaded installer.
inline constexpr base::FeatureParam<std::string> kDownloadedInstallerFileName(
    &kInstallerDownloader,
    "downloaded_installer_file_name",
    "ChromeSetup.exe");

}  // namespace installer_downloader

#endif  // CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_FEATURE_H_
