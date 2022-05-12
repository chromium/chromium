// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PAGE_HANDLER_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_model.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

using read_anything::mojom::ContentNodePtr;
using read_anything::mojom::Page;
using read_anything::mojom::PageHandler;

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingPageHandler
//
//  A handler of the Read Anything app
//  (chrome/browser/resources/side_panel/read_anything/app.ts).
//  This class is created and owned by ReadAnythingUI and has the same lifetime
//  as the Side Panel view.
//
class ReadAnythingPageHandler : public PageHandler,
                                public ReadAnythingModel::Observer,
                                public ReadAnythingCoordinator::Observer {
 public:
  class Delegate {
   public:
    virtual void OnUIReady() = 0;
  };

  ReadAnythingPageHandler(mojo::PendingRemote<Page> page,
                          mojo::PendingReceiver<PageHandler> receiver);
  ReadAnythingPageHandler(const ReadAnythingPageHandler&) = delete;
  ReadAnythingPageHandler& operator=(const ReadAnythingPageHandler&) = delete;
  ~ReadAnythingPageHandler() override;

  // PageHandler:
  void OnUIReady() override;

  // ReadAnythingModel::Observer:
  void OnFontNameUpdated(const std::string& new_font_name) override;
  void OnContentUpdated(
      const std::vector<ContentNodePtr>& content_nodes) override;

  // ReadAnythingCoordinator::Observer:
  void OnCoordinatorDestroyed() override;

 private:
  raw_ptr<ReadAnythingCoordinator> coordinator_;
  raw_ptr<ReadAnythingModel> model_;
  raw_ptr<ReadAnythingPageHandler::Delegate> delegate_;

  Browser* browser_;

  mojo::Receiver<PageHandler> receiver_;
  mojo::Remote<Page> page_;
  base::WeakPtrFactory<ReadAnythingPageHandler> weak_pointer_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PAGE_HANDLER_H_
