// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_manager.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

WebAppInstallManager::WebAppInstallManager(Profile* profile,
                                           WebAppRegistrar* registrar)
    : profile_(profile),
      registrar_(registrar),
      data_retriever_(std::make_unique<WebAppDataRetriever>()) {
  DCHECK(AllowWebAppInstallation(profile_));
}

WebAppInstallManager::~WebAppInstallManager() = default;

bool WebAppInstallManager::CanInstallWebApp(
    const content::WebContents* web_contents) {
  return IsValidWebAppUrl(web_contents->GetURL());
}

void WebAppInstallManager::InstallWebApp(content::WebContents* web_contents,
                                         bool force_shortcut_app,
                                         OnceInstallCallback install_callback) {
  // TODO(loyso): Use force_shortcut_app flag during installation.

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  data_retriever_->GetWebApplicationInfo(
      web_contents,
      base::BindOnce(&WebAppInstallManager::OnGetWebApplicationInfo,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(install_callback)));
}

void WebAppInstallManager::SetDataRetrieverForTesting(
    std::unique_ptr<WebAppDataRetriever> data_retriever) {
  data_retriever_ = std::move(data_retriever);
}

void WebAppInstallManager::OnGetWebApplicationInfo(
    OnceInstallCallback install_callback,
    std::unique_ptr<WebApplicationInfo> web_app_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!web_app_info) {
    std::move(install_callback)
        .Run(AppId(), InstallResultCode::kGetWebApplicationInfoFailed);
    return;
  }

  // TODO(loyso): Implement installation logic from BookmarkAppHelper:
  // - InstallableManager to get InstallableData.
  // - UpdateWebAppInfoFromManifest.
  // - UpdateShareTargetInPrefs.
  // - WebAppIconDownloader.
  // etc

  const AppId app_id = GenerateAppIdFromURL(web_app_info->app_url);
  auto web_app = std::make_unique<WebApp>(app_id);

  web_app->SetName(base::UTF16ToUTF8(web_app_info->title));
  web_app->SetDescription(base::UTF16ToUTF8(web_app_info->description));
  web_app->SetLaunchUrl(web_app_info->app_url.spec());

  registrar_->RegisterApp(std::move(web_app));

  std::move(install_callback).Run(app_id, InstallResultCode::kSuccess);
}

}  // namespace web_app
