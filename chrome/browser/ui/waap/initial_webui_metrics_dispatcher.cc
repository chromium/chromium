// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/initial_webui_metrics_dispatcher.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "content/public/browser/web_contents.h"

// static
InitialWebUIWebContentsMetricsDispatcher*
InitialWebUIMetricsDispatcher::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  InitialWebUIWebContentsMetricsDispatcher::CreateForWebContents(web_contents);
  return content::WebContentsUserData<
      InitialWebUIWebContentsMetricsDispatcher>::FromWebContents(web_contents);
}

InitialWebUIMetricsDispatcher::InitialWebUIMetricsDispatcher() = default;

InitialWebUIMetricsDispatcher::~InitialWebUIMetricsDispatcher() = default;

void InitialWebUIMetricsDispatcher::OnCreated() {}

void InitialWebUIMetricsDispatcher::OnCommit(base::TimeTicks init_time,
                                             base::TimeTicks launched_time) {}

void InitialWebUIMetricsDispatcher::OnFirstPaint(base::TimeTicks paint_time) {}

void InitialWebUIMetricsDispatcher::OnFirstContentfulPaint(
    base::TimeTicks fcp_time) {}

void InitialWebUIMetricsDispatcher::FlushMetricsToManager() {}

// InitialWebUIWebContentsMetricsDispatcher:
WEB_CONTENTS_USER_DATA_KEY_IMPL(InitialWebUIWebContentsMetricsDispatcher);

InitialWebUIWebContentsMetricsDispatcher::
    InitialWebUIWebContentsMetricsDispatcher(content::WebContents* web_contents)
    : content::WebContentsUserData<InitialWebUIWebContentsMetricsDispatcher>(
          *web_contents) {}

InitialWebUIWebContentsMetricsDispatcher::
    ~InitialWebUIWebContentsMetricsDispatcher() = default;

WaapUIMetricsService* InitialWebUIWebContentsMetricsDispatcher::GetService() {
  return WaapUIMetricsService::Get(
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext()));
}

InitialWebUIWindowMetricsManager*
InitialWebUIWebContentsMetricsDispatcher::GetWindowMetricsManager() {
  return InitialWebUIWindowMetricsManager::From(
      webui::GetBrowserWindowInterface(&GetWebContents()));
}

// InitialWebUINativeMetricsDispatcher:
InitialWebUINativeMetricsDispatcher::InitialWebUINativeMetricsDispatcher(
    Profile* profile,
    InitialWebUIWindowMetricsManager* window_metrics_manager)
    : profile_(profile), window_metrics_manager_(window_metrics_manager) {}

InitialWebUINativeMetricsDispatcher::~InitialWebUINativeMetricsDispatcher() =
    default;

WaapUIMetricsService* InitialWebUINativeMetricsDispatcher::GetService() {
  return profile_ ? WaapUIMetricsService::Get(profile_) : nullptr;
}

InitialWebUIWindowMetricsManager*
InitialWebUINativeMetricsDispatcher::GetWindowMetricsManager() {
  return window_metrics_manager_;
}
