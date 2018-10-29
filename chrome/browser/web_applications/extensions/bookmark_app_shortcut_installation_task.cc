// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_shortcut_installation_task.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_installer.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

BookmarkAppShortcutInstallationTask::BookmarkAppShortcutInstallationTask(
    Profile* profile)
    : BookmarkAppInstallationTask(
          profile,
          // Pass an empty AppInfo since it doesn't influence the installation
          // right now.
          // TODO(crbug.com/864904): Take an AppInfo object once the installer
          // can use the information.
          web_app::PendingAppManager::AppInfo(
              GURL(),
              web_app::LaunchContainer::kTab,
              web_app::InstallSource::kInternal)) {}

BookmarkAppShortcutInstallationTask::~BookmarkAppShortcutInstallationTask() =
    default;

void BookmarkAppShortcutInstallationTask::InstallFromWebContents(
    content::WebContents* web_contents,
    ResultCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  data_retriever().GetWebApplicationInfo(
      web_contents,
      base::BindOnce(
          &BookmarkAppShortcutInstallationTask::OnGetWebApplicationInfo,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BookmarkAppShortcutInstallationTask::OnGetWebApplicationInfo(
    ResultCallback result_callback,
    std::unique_ptr<WebApplicationInfo> web_app_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_app_info) {
    std::move(result_callback)
        .Run(Result(web_app::InstallResultCode::kGetWebApplicationInfoFailed,
                    base::nullopt));
    return;
  }

  // TODO(crbug.com/864904): Retrieve the Manifest before downloading icons.

  std::vector<GURL> icon_urls;
  for (const auto& icon : web_app_info->icons) {
    icon_urls.push_back(icon.url);
  }

  data_retriever().GetIcons(
      web_app_info->app_url, icon_urls,
      base::BindOnce(&BookmarkAppShortcutInstallationTask::OnGetIcons,
                     weak_ptr_factory_.GetWeakPtr(), std::move(result_callback),
                     std::move(web_app_info)));
}

void BookmarkAppShortcutInstallationTask::OnGetIcons(
    ResultCallback result_callback,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    std::vector<WebApplicationInfo::IconInfo> icons) {
  web_app_info->icons = std::move(icons);

  // TODO(crbug.com/864904): Make this a WebContents observer and cancel the
  // task if the WebContents has been destroyed.
  installer().Install(
      *web_app_info,
      base::BindOnce(&BookmarkAppShortcutInstallationTask::OnInstalled,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(result_callback)));
}

void BookmarkAppShortcutInstallationTask::OnInstalled(
    ResultCallback result_callback,
    const std::string& app_id) {
  if (app_id.empty()) {
    std::move(result_callback)
        .Run(Result(web_app::InstallResultCode::kFailedUnknownReason,
                    base::nullopt));
    return;
  }
  std::move(result_callback)
      .Run(Result(web_app::InstallResultCode::kSuccess, app_id));
}

}  // namespace extensions
