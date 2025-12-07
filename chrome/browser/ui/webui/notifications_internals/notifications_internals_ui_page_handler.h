// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NOTIFICATIONS_INTERNALS_NOTIFICATIONS_INTERNALS_UI_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NOTIFICATIONS_INTERNALS_NOTIFICATIONS_INTERNALS_UI_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "chrome/browser/ui/webui/notifications_internals/notifications_internals.mojom.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

// Class acting as a controller of the chrome://notifications-internals WebUI.
class NotificationsInternalsUIPageHandler
    : notifications_internals::mojom::PageHandler {
 public:
  NotificationsInternalsUIPageHandler(
      mojo::PendingReceiver<notifications_internals::mojom::PageHandler>
          receiver,
      notifications::NotificationScheduleService* service,
      PrefService* pref_service);

  NotificationsInternalsUIPageHandler(
      const NotificationsInternalsUIPageHandler&) = delete;
  NotificationsInternalsUIPageHandler& operator=(
      const NotificationsInternalsUIPageHandler&) = delete;

  ~NotificationsInternalsUIPageHandler() override;

  // notifications_internals::mojom::PageHandler
  void ScheduleNotification(const std::string& feature_type) override;

 private:
  mojo::Receiver<notifications_internals::mojom::PageHandler> receiver_;

  raw_ptr<notifications::NotificationScheduleService> service_;

  raw_ptr<PrefService> pref_service_;

  base::WeakPtrFactory<NotificationsInternalsUIPageHandler> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NOTIFICATIONS_INTERNALS_NOTIFICATIONS_INTERNALS_UI_PAGE_HANDLER_H_
