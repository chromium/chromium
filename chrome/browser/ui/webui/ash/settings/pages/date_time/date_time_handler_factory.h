// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DATE_TIME_DATE_TIME_HANDLER_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DATE_TIME_DATE_TIME_HANDLER_FACTORY_H_

#include <memory>

#include "chrome/browser/ui/webui/ash/settings/pages/date_time/mojom/date_time_handler.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/pages/date_time/new_date_time_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace ash::settings {

class DateTimeHandlerFactory : public date_time::mojom::PageHandlerFactory {
 public:
  explicit DateTimeHandlerFactory(
      content::WebUI* web_ui,
      Profile* profile,
      mojo::PendingReceiver<date_time::mojom::PageHandlerFactory> receiver);

  DateTimeHandlerFactory(const DateTimeHandlerFactory&) = delete;
  DateTimeHandlerFactory& operator=(const DateTimeHandlerFactory&) = delete;

  ~DateTimeHandlerFactory() override;

 private:
  // date_time::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<date_time::mojom::Page> page,
      mojo::PendingReceiver<date_time::mojom::PageHandler> receiver) override;

  const raw_ptr<content::WebUI> web_ui_;
  const raw_ptr<Profile> profile_;

  std::unique_ptr<NewDateTimeHandler> page_handler_;
  mojo::Receiver<date_time::mojom::PageHandlerFactory> page_factory_receiver_{
      this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DATE_TIME_DATE_TIME_HANDLER_FACTORY_H_
