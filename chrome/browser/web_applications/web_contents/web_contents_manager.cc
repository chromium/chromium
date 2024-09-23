// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"

#include <memory>

#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_app_icon_downloader.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"

namespace web_app {

WebContentsManager::WebContentsManager() = default;
WebContentsManager::~WebContentsManager() = default;

std::unique_ptr<webapps::WebAppUrlLoader>
WebContentsManager::CreateUrlLoader() {
  return std::make_unique<webapps::WebAppUrlLoader>();
}

std::unique_ptr<WebAppDataRetriever> WebContentsManager::CreateDataRetriever() {
  return std::make_unique<WebAppDataRetriever>();
}

std::unique_ptr<WebAppIconDownloader>
WebContentsManager::CreateIconDownloader() {
  return std::make_unique<WebAppIconDownloader>();
}

base::WeakPtr<WebContentsManager> WebContentsManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace web_app
