// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/download_directory_override_manager.h"

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

DownloadDirectoryOverrideManager::DownloadDirectoryOverrideManager(
    DevToolsClient* client)
    : client_(client), is_connected_(false) {
  client_->AddListener(this);
}

DownloadDirectoryOverrideManager::~DownloadDirectoryOverrideManager() = default;

Status DownloadDirectoryOverrideManager::OverrideDownloadDirectoryWhenConnected(
    const std::string& new_download_directory) {
  download_directory_ = std::make_unique<std::string>(new_download_directory);
  if (is_connected_)
    return ApplyOverride();
  return Status(kOk);
}

Status DownloadDirectoryOverrideManager::OnConnected(DevToolsClient* client) {
  is_connected_ = true;
  if (download_directory_)
    return ApplyOverride();
  return Status(kOk);
}

Status DownloadDirectoryOverrideManager::ApplyOverride() {
  base::DictionaryValue params;
  params.SetString("behavior", "allow");
  params.SetString("downloadPath", *download_directory_);
  return client_->SendCommand("Page.setDownloadBehavior", params);
}
