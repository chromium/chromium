// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/install_finalizer.h"

#include "base/logging.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"

namespace web_app {

void InstallFinalizer::SetSubsystems(AppRegistrar* registrar,
                                     WebAppUiManager* ui_manager) {
  registrar_ = registrar;
  ui_manager_ = ui_manager;
}

bool InstallFinalizer::CanAddAppToQuickLaunchBar() const {
  return ui_manager().CanAddAppToQuickLaunchBar();
}

void InstallFinalizer::AddAppToQuickLaunchBar(const AppId& app_id) {
  ui_manager().AddAppToQuickLaunchBar(app_id);
}

bool InstallFinalizer::CanReparentTab(const AppId& app_id,
                                      bool shortcut_created) const {
  // Reparent the web contents into its own window only if that is the
  // app's launch type.
  DCHECK(registrar_);
  if (registrar_->GetAppUserDisplayMode(app_id) != DisplayMode::kStandalone)
    return false;

  return ui_manager().CanReparentAppTabToWindow(app_id, shortcut_created);
}

void InstallFinalizer::ReparentTab(const AppId& app_id,
                                   bool shortcut_created,
                                   content::WebContents* web_contents) {
  DCHECK(web_contents);
  return ui_manager().ReparentAppTabToWindow(web_contents, app_id,
                                             shortcut_created);
}

}  // namespace web_app
