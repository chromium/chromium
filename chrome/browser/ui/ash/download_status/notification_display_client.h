// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_NOTIFICATION_DISPLAY_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_NOTIFICATION_DISPLAY_CLIENT_H_

#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/ash/download_status/display_client.h"

class Profile;

namespace ash::download_status {

struct DisplayMetadata;

// The client to display download notifications. Created only when the downloads
// integration V2 feature is enabled.
class NotificationDisplayClient : public DisplayClient {
 public:
  explicit NotificationDisplayClient(Profile* profile);
  NotificationDisplayClient(const NotificationDisplayClient&) = delete;
  NotificationDisplayClient& operator=(const NotificationDisplayClient&) =
      delete;
  ~NotificationDisplayClient() override;

 private:
  // DisplayClient:
  void AddOrUpdate(const std::string& guid,
                   const DisplayMetadata& display_metadata) override;
  void Remove(const std::string& guid) override;

  // Called when the notification associated with `guid` is closed by user.
  void OnNotificationClosedByUser(const std::string& guid);

  // A collection of the guids associated with the notifications closed by user.
  // Used to prevent a notification closed by user from showing again.
  // Add a new guid to this collection when a notification is closed by user.
  // Remove a guid from this collection when the associated download ends.
  std::set<std::string> notifications_closed_by_user_guids_;

  base::WeakPtrFactory<NotificationDisplayClient> weak_ptr_factory_{this};
};

}  // namespace ash::download_status

#endif  // CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_NOTIFICATION_DISPLAY_CLIENT_H_
