// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_installation_task.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_installer.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace {

std::unique_ptr<BookmarkAppHelper> BookmarkAppHelperCreateWrapper(
    Profile* profile,
    const WebApplicationInfo& web_app_info,
    content::WebContents* web_contents,
    WebappInstallSource install_source) {
  return std::make_unique<BookmarkAppHelper>(profile, web_app_info,
                                             web_contents, install_source);
}

}  // namespace

BookmarkAppInstallationTask::Result::Result(web_app::InstallResultCode code,
                                            base::Optional<std::string> app_id)
    : code(code), app_id(std::move(app_id)) {
  DCHECK_EQ(code == web_app::InstallResultCode::kSuccess, app_id.has_value());
}

BookmarkAppInstallationTask::Result::Result(Result&&) = default;

BookmarkAppInstallationTask::Result::~Result() = default;

// static
void BookmarkAppInstallationTask::CreateTabHelpers(
    content::WebContents* web_contents) {
  InstallableManager::CreateForWebContents(web_contents);
  SecurityStateTabHelper::CreateForWebContents(web_contents);
  favicon::CreateContentFaviconDriverForWebContents(web_contents);
}

BookmarkAppInstallationTask::BookmarkAppInstallationTask(
    Profile* profile,
    web_app::PendingAppManager::AppInfo app_info)
    : profile_(profile),
      app_info_(std::move(app_info)),
      helper_factory_(base::BindRepeating(&BookmarkAppHelperCreateWrapper)),
      data_retriever_(std::make_unique<web_app::WebAppDataRetriever>()),
      installer_(std::make_unique<BookmarkAppInstaller>(profile)) {}

BookmarkAppInstallationTask::~BookmarkAppInstallationTask() = default;

void BookmarkAppInstallationTask::InstallWebAppOrShortcutFromWebContents(
    content::WebContents* web_contents,
    ResultCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  data_retriever().GetWebApplicationInfo(
      web_contents,
      base::BindOnce(&BookmarkAppInstallationTask::OnGetWebApplicationInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     web_contents));
}

void BookmarkAppInstallationTask::SetBookmarkAppHelperFactoryForTesting(
    BookmarkAppHelperFactory helper_factory) {
  helper_factory_ = helper_factory;
}

void BookmarkAppInstallationTask::SetDataRetrieverForTesting(
    std::unique_ptr<web_app::WebAppDataRetriever> data_retriever) {
  data_retriever_ = std::move(data_retriever);
}

void BookmarkAppInstallationTask::SetInstallerForTesting(
    std::unique_ptr<BookmarkAppInstaller> installer) {
  installer_ = std::move(installer);
}

void BookmarkAppInstallationTask::OnGetWebApplicationInfo(
    ResultCallback result_callback,
    content::WebContents* web_contents,
    std::unique_ptr<WebApplicationInfo> web_app_info) {
  if (!web_app_info) {
    std::move(result_callback)
        .Run(Result(web_app::InstallResultCode::kGetWebApplicationInfoFailed,
                    std::string()));
    return;
  }

  // TODO(crbug.com/864904): Use an appropriate install source once source
  // is plumbed through this class.
  helper_ = helper_factory_.Run(profile_, *web_app_info, web_contents,
                                WebappInstallSource::MENU_BROWSER_TAB);

  switch (app_info_.launch_container) {
    case web_app::LaunchContainer::kDefault:
      break;
    case web_app::LaunchContainer::kTab:
      helper_->set_forced_launch_type(LAUNCH_TYPE_REGULAR);
      break;
    case web_app::LaunchContainer::kWindow:
      helper_->set_forced_launch_type(LAUNCH_TYPE_WINDOW);
      break;
  }

  switch (app_info_.install_source) {
    // TODO(nigeltao/ortuno): should these two cases lead to different
    // Manifest::Location values: INTERNAL vs EXTERNAL_PREF_DOWNLOAD?
    case web_app::InstallSource::kInternal:
    case web_app::InstallSource::kExternalDefault:
      helper_->set_is_default_app();
      break;
    case web_app::InstallSource::kExternalPolicy:
      helper_->set_is_policy_installed_app();
      break;
    case web_app::InstallSource::kSystemInstalled:
      helper_->set_is_system_app();
      break;
  }

  if (!app_info_.create_shortcuts)
    helper_->set_skip_shortcut_creation();

  if (app_info_.bypass_service_worker_check)
    helper_->set_bypass_service_worker_check();

  if (app_info_.require_manifest)
    helper_->set_require_manifest();

  helper_->Create(base::Bind(&BookmarkAppInstallationTask::OnInstalled,
                             weak_ptr_factory_.GetWeakPtr(),
                             base::Passed(&result_callback)));
}

void BookmarkAppInstallationTask::OnInstalled(
    ResultCallback result_callback,
    const Extension* extension,
    const WebApplicationInfo& web_app_info) {
  std::move(result_callback)
      .Run(extension
               ? Result(web_app::InstallResultCode::kSuccess, extension->id())
               : Result(web_app::InstallResultCode::kFailedUnknownReason,
                        base::nullopt));
}

}  // namespace extensions
