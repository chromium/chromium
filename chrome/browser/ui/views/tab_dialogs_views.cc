// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_dialogs_views.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"
#include "chrome/browser/ui/views/hung_renderer_view.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog_controller.h"
#include "content/public/browser/web_contents.h"

#if defined(TOOLKIT_VIEWS) && !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/views/web_apps/deprecated_apps_dialog_view.h"
#include "chrome/browser/ui/views/web_apps/force_installed_deprecated_apps_dialog_view.h"
#include "chrome/browser/ui/views/web_apps/force_installed_preinstalled_deprecated_app_dialog_view.h"
#endif

// static
void TabDialogs::CreateForWebContents(content::WebContents* contents) {
  DCHECK(contents);
  if (!FromWebContents(contents)) {
    contents->SetUserData(UserDataKey(),
                          std::make_unique<TabDialogsViews>(contents));
  }
}

TabDialogsViews::TabDialogsViews(content::WebContents* contents)
    : web_contents_(contents) {
  DCHECK(contents);
}

TabDialogsViews::~TabDialogsViews() = default;

gfx::NativeView TabDialogsViews::GetDialogParentView() const {
  return web_contents_->GetNativeView();
}

void TabDialogsViews::ShowCollectedCookies() {
  PageSpecificSiteDataDialogController::CreateAndShowForWebContents(
      web_contents_);
}

void TabDialogsViews::ShowHungRendererDialog(
    content::RenderWidgetHost* render_widget_host,
    base::RepeatingClosure hang_monitor_restarter) {
  HungRendererDialogView::Show(web_contents_, render_widget_host,
                               std::move(hang_monitor_restarter));
}

void TabDialogsViews::HideHungRendererDialog(
    content::RenderWidgetHost* render_widget_host) {
  HungRendererDialogView::Hide(web_contents_, render_widget_host);
}

bool TabDialogsViews::IsShowingHungRendererDialog() {
  return HungRendererDialogView::IsShowingForWebContents(web_contents_);
}

void TabDialogsViews::ShowManagePasswordsBubble(bool user_action) {
  if (PasswordBubbleViewBase::manage_password_bubble()) {
    // The bubble is currently shown for some other tab. We should close it now
    // and open for |web_contents_|.
    PasswordBubbleViewBase::CloseCurrentBubble();
  }
  PasswordBubbleViewBase::ShowBubble(
      web_contents_, user_action ? LocationBarBubbleDelegateView::USER_GESTURE
                                 : LocationBarBubbleDelegateView::AUTOMATIC);
}

void TabDialogsViews::HideManagePasswordsBubble() {
  PasswordBubbleViewBase* bubble =
      PasswordBubbleViewBase::manage_password_bubble();
  if (!bubble)
    return;
  if (bubble->GetWebContents() == web_contents_)
    PasswordBubbleViewBase::CloseCurrentBubble();
}

void TabDialogsViews::ShowDeprecatedAppsDialog(
    const extensions::ExtensionId& optional_launched_extension_id,
    const std::set<extensions::ExtensionId>& deprecated_app_ids,
    content::WebContents* web_contents) {
#if defined(TOOLKIT_VIEWS) && !BUILDFLAG(IS_CHROMEOS)
  DeprecatedAppsDialogView::CreateAndShowDialog(
      optional_launched_extension_id, deprecated_app_ids, web_contents);
#endif
}

void TabDialogsViews::ShowForceInstalledDeprecatedAppsDialog(
    const extensions::ExtensionId& app_id,
    content::WebContents* web_contents) {
#if defined(TOOLKIT_VIEWS) && !BUILDFLAG(IS_CHROMEOS)
  ForceInstalledDeprecatedAppsDialogView::CreateAndShowDialog(app_id,
                                                              web_contents);
#endif
}

void TabDialogsViews::ShowForceInstalledPreinstalledDeprecatedAppDialog(
    const extensions::ExtensionId& extension_id,
    content::WebContents* web_contents) {
#if defined(TOOLKIT_VIEWS) && !BUILDFLAG(IS_CHROMEOS)
  ForceInstalledPreinstalledDeprecatedAppDialogView::CreateAndShowDialog(
      extension_id, web_contents);
#endif
}
