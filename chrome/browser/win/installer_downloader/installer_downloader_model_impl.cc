// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/installer_downloader/installer_downloader_model_impl.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_pref_names.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace installer_downloader {

InstallerDownloaderModelImpl::InstallerDownloaderModelImpl() = default;

InstallerDownloaderModelImpl::~InstallerDownloaderModelImpl() = default;

void InstallerDownloaderModelImpl::CheckEligibility(
    base::OnceCallback<void(const std::optional<base::FilePath>& destination)>
        callback) {}

void InstallerDownloaderModelImpl::StartDownload(
    const GURL& url,
    const base::FilePath& dest,
    CompletionCallback completion_callback) {}

bool InstallerDownloaderModelImpl::IsMaxShowCountReached() const {
  return g_browser_process->local_state()->GetInteger(
             prefs::kInstallerDownloaderInfobarShowCount) >= kMaxShowCount;
}

}  // namespace installer_downloader
