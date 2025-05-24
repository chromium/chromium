// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/installer_downloader/installer_downloader_feature.h"

#include "base/feature_list.h"

namespace installer_downloader {

BASE_FEATURE(kInstallerDownloader,
             "InstallerDownloader",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace installer_downloader
