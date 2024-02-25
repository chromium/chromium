// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SERVICES_HATS_OS_SETTINGS_HATS_MANAGER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SERVICES_HATS_OS_SETTINGS_HATS_MANAGER_H_

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {

class HatsNotificationController;

namespace settings {

// Manager for the Chrome OS Settings Hats. This class is implemented as a
// KeyedService, so one instance of the class is intended to be active for the
// lifetime of a logged-in user, even if the settings app is not opened.
//
// Handles triggering HaTS surveys based on user interaction with settings
// features.
class OsSettingsHatsManager : public KeyedService {
 public:
  explicit OsSettingsHatsManager(content::BrowserContext* context);

  OsSettingsHatsManager(const OsSettingsHatsManager& other) = delete;
  OsSettingsHatsManager& operator=(const OsSettingsHatsManager& other) = delete;

  ~OsSettingsHatsManager() override;

  // Sends Settings Hats Notification if a user is chosen as a candidate.
  virtual void MaybeSendSettingsHats();

  // Sets whether product_specific_data needs to get recorded if the user has
  // used Settings Search.
  virtual void SetSettingsUsedSearch(bool set_search);

 private:
  raw_ptr<content::BrowserContext> context_;
  base::OneShotTimer hats_timer_;
  scoped_refptr<HatsNotificationController> hats_notification_controller_;
  bool has_user_used_search = false;
};

}  // namespace settings
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SERVICES_HATS_OS_SETTINGS_HATS_MANAGER_H_
