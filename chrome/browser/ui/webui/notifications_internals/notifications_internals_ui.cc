// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/notifications_internals/notifications_internals_ui.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/notifications/scheduler/notification_schedule_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/ui/webui/notifications_internals/notifications_internals.mojom.h"
#include "chrome/browser/ui/webui/notifications_internals/notifications_internals_ui_page_handler.h"
#include "chrome/grit/notifications_internals_resources.h"
#include "chrome/grit/notifications_internals_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/webui_util.h"

bool NotificationsInternalsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return !profile->IsOffTheRecord();
}

NotificationsInternalsUI::NotificationsInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui), profile_(Profile::FromWebUI(web_ui)) {
  // chrome://notifications-internals source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile_, chrome::kChromeUINotificationsInternalsHost);
  webui::SetupWebUIDataSource(
      source, kNotificationsInternalsResources,
      IDR_NOTIFICATIONS_INTERNALS_NOTIFICATIONS_INTERNALS_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(NotificationsInternalsUI)

NotificationsInternalsUI::~NotificationsInternalsUI() = default;

void NotificationsInternalsUI::BindInterface(
    mojo::PendingReceiver<notifications_internals::mojom::PageHandler>
        receiver) {
  page_handler_ = std::make_unique<NotificationsInternalsUIPageHandler>(
      std::move(receiver),
      NotificationScheduleServiceFactory::GetForKey(profile_->GetProfileKey()),
      profile_->GetPrefs());
}
