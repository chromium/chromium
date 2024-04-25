// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_PAGE_HANDLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash::extended_updates {

class ExtendedUpdatesPageHandler
    : public ash::extended_updates::mojom::PageHandler {
 public:
  ExtendedUpdatesPageHandler(
      mojo::PendingRemote<ash::extended_updates::mojom::Page> page,
      mojo::PendingReceiver<ash::extended_updates::mojom::PageHandler> receiver,
      content::WebUI* web_ui,
      base::OnceClosure close_dialog_callback);

  ExtendedUpdatesPageHandler(const ExtendedUpdatesPageHandler&) = delete;
  ExtendedUpdatesPageHandler& operator=(const ExtendedUpdatesPageHandler&) =
      delete;

  ~ExtendedUpdatesPageHandler() override;

  // ash::extended_updates::mojom::PageHandler:
  void OptInToExtendedUpdates(OptInToExtendedUpdatesCallback callback) override;
  void CloseDialog() override;
  void GetInstalledAndroidApps(
      GetInstalledAndroidAppsCallback callback) override;

 private:
  mojo::Remote<ash::extended_updates::mojom::Page> page_;
  mojo::Receiver<ash::extended_updates::mojom::PageHandler> receiver_;

  raw_ptr<content::WebUI> web_ui_ = nullptr;
  base::OnceClosure close_dialog_callback_;
};

}  // namespace ash::extended_updates

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_PAGE_HANDLER_H_
