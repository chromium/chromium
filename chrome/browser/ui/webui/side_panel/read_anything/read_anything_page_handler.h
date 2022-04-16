// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PAGE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_model.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class ReadAnythingPageHandler : public read_anything::mojom::PageHandler,
                                public ReadAnythingModel::Observer {
 public:
  class Delegate {
   public:
    virtual void OnUIShown() = 0;
  };

  explicit ReadAnythingPageHandler(
      mojo::PendingRemote<read_anything::mojom::Page> page,
      mojo::PendingReceiver<read_anything::mojom::PageHandler> receiver);
  ReadAnythingPageHandler(const ReadAnythingPageHandler&) = delete;
  ReadAnythingPageHandler& operator=(const ReadAnythingPageHandler&) = delete;
  ~ReadAnythingPageHandler() override;

  // read_anything::mojom::PageHandler:
  void ShowUI() override;

  // ReadAnythingModel::Observer:
  void OnFontNameUpdated(const std::string& new_font_name) override;
  void OnContentUpdated(std::vector<std::string> content) override;

 private:
  raw_ptr<ReadAnythingPageHandler::Delegate> delegate_;

  Browser* browser_;

  mojo::Receiver<read_anything::mojom::PageHandler> receiver_;
  mojo::Remote<read_anything::mojom::Page> page_;
  base::WeakPtrFactory<ReadAnythingPageHandler> weak_pointer_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PAGE_HANDLER_H_
