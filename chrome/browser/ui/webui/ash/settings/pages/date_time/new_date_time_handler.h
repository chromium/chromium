// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DATE_TIME_NEW_DATE_TIME_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DATE_TIME_NEW_DATE_TIME_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/ash/settings/pages/date_time/mojom/date_time_handler.mojom.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_client.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace ash::settings {

// ChromeOS "Date and time" settings UI handler.
class NewDateTimeHandler : public date_time::mojom::PageHandler,
                           public SystemClockClient::Observer {
 public:
  NewDateTimeHandler(
      mojo::PendingReceiver<date_time::mojom::PageHandler> receiver,
      mojo::PendingRemote<date_time::mojom::Page> page,
      content::WebUI* web_ui,
      Profile* profile);

  NewDateTimeHandler(const NewDateTimeHandler&) = delete;
  NewDateTimeHandler& operator=(const NewDateTimeHandler&) = delete;

  ~NewDateTimeHandler() override;

 private:
  // date_time::mojom::PageHandler:
  void ShowParentAccessForTimezone() override;
  void GetTimezones(GetTimezonesCallback callback) override;
  void ShowSetDateTimeUI() override;

  // SystemClockClient::Observer implementation.
  void SystemClockCanSetTimeChanged(bool can_set_time) override;

  void OnParentAccessValidation(bool success);

  const raw_ptr<content::WebUI> web_ui_;
  const raw_ptr<Profile> profile_;

  mojo::Remote<date_time::mojom::Page> page_;
  mojo::Receiver<date_time::mojom::PageHandler> receiver_{this};

  base::ScopedObservation<SystemClockClient, SystemClockClient::Observer>
      scoped_observation_{this};
  base::WeakPtrFactory<NewDateTimeHandler> weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DATE_TIME_NEW_DATE_TIME_HANDLER_H_
