// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_PAGE_HANDLER_H_

#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::extended_updates {

class ExtendedUpdatesPageHandler
    : public ash::extended_updates::mojom::PageHandler {
 public:
  ExtendedUpdatesPageHandler(
      mojo::PendingRemote<ash::extended_updates::mojom::Page> page,
      mojo::PendingReceiver<ash::extended_updates::mojom::PageHandler>
          receiver);

  ExtendedUpdatesPageHandler(const ExtendedUpdatesPageHandler&) = delete;
  ExtendedUpdatesPageHandler& operator=(const ExtendedUpdatesPageHandler&) =
      delete;

  ~ExtendedUpdatesPageHandler() override;

  // ash::extended_updates::mojom::PageHandler:
  void OptInToExtendedUpdates(OptInToExtendedUpdatesCallback callback) override;

 private:
  mojo::Remote<ash::extended_updates::mojom::Page> page_;
  mojo::Receiver<ash::extended_updates::mojom::PageHandler> receiver_;
};

}  // namespace ash::extended_updates

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_PAGE_HANDLER_H_
