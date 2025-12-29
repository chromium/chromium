// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_UPDATER_UPDATER_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_UPDATER_UPDATER_PAGE_HANDLER_H_

#include "chrome/browser/ui/webui/updater/updater_ui.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class UpdaterPageHandler final : public updater_ui::mojom::PageHandler {
 public:
  UpdaterPageHandler(
      mojo::PendingReceiver<updater_ui::mojom::PageHandler> receiver,
      mojo::PendingRemote<updater_ui::mojom::Page> page);

  UpdaterPageHandler(const UpdaterPageHandler&) = delete;
  UpdaterPageHandler& operator=(const UpdaterPageHandler&) = delete;

  ~UpdaterPageHandler() override;

  void GetAllUpdaterEvents(GetAllUpdaterEventsCallback callback) override;

 private:
  mojo::Receiver<updater_ui::mojom::PageHandler> receiver_;
  mojo::Remote<updater_ui::mojom::Page> page_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_UPDATER_UPDATER_PAGE_HANDLER_H_
