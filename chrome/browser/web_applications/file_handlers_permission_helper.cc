// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/file_handlers_permission_helper.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/manifest_update_task.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "url/gurl.h"

namespace web_app {

FileHandlersPermissionHelper::FileHandlersPermissionHelper(
    WebAppInstallFinalizer* finalizer)
    : finalizer_(finalizer) {}

FileHandlerUpdateAction FileHandlersPermissionHelper::WillUpdateApp(
    const WebApp& web_app,
    const WebApplicationInfo& web_app_info) {
  if (!finalizer_->os_integration_manager().IsFileHandlingAPIAvailable(
          web_app.app_id()))
    return FileHandlerUpdateAction::kNoUpdate;

  const GURL& url = web_app_info.scope;

  // Keep in sync with chromeos::kChromeUIMediaAppURL.
  const char kChromeUIMediaAppURL[] = "chrome://media-app/";
  // Keep in sync with chromeos::kChromeUICameraAppURL.
  const char kChromeUICameraAppURL[] = "chrome://camera-app/";

  // Omit file handler removal and permission downgrade for the ChromeOS Media
  // and Camera System Web Apps (SWAs), which have permissions granted by
  // default. This exception and check is only relevant in ChromeOS, the only
  // platform where SWAs are in use.
  if (url == kChromeUIMediaAppURL || url == kChromeUICameraAppURL)
    return FileHandlerUpdateAction::kUpdate;

  if (web_app.file_handler_approval_state() == ApiApprovalState::kDisallowed)
    return FileHandlerUpdateAction::kNoUpdate;

  // TODO(https://crbug.com/1197013): Consider trying to re-use the comparison
  // results from the ManifestUpdateTask.
  const apps::FileHandlers* old_handlers =
      finalizer_->registrar().GetAppFileHandlers(web_app.app_id());
  DCHECK(old_handlers);
  if (*old_handlers == web_app_info.file_handlers)
    return FileHandlerUpdateAction::kNoUpdate;

  return FileHandlerUpdateAction::kUpdate;
}

}  // namespace web_app
