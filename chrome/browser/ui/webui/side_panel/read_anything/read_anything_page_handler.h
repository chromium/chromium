// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PAGE_HANDLER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_model.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_update_forward.h"

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingPageHandler
//
//  A handler of the Read Anything app
//  (chrome/browser/resources/side_panel/read_anything/app.ts).
//  This class is created and owned by ReadAnythingUI and has the same lifetime
//  as the Side Panel view.
//
class ReadAnythingPageHandler : public read_anything::mojom::PageHandler,
                                public ReadAnythingModel::Observer,
                                public ReadAnythingCoordinator::Observer {
 public:
  class Delegate {
   public:
    virtual void OnUIReady() = 0;
    virtual void OnUIDestroyed() = 0;
  };

  ReadAnythingPageHandler(
      mojo::PendingRemote<read_anything::mojom::Page> page,
      mojo::PendingReceiver<read_anything::mojom::PageHandler> receiver);
  ReadAnythingPageHandler(const ReadAnythingPageHandler&) = delete;
  ReadAnythingPageHandler& operator=(const ReadAnythingPageHandler&) = delete;
  ~ReadAnythingPageHandler() override;

  // ReadAnythingModel::Observer:
  void OnAXTreeDistilled(
      const ui::AXTreeUpdate& snapshot,
      const std::vector<ui::AXNodeID>& content_node_ids) override;
  void OnReadAnythingThemeChanged(
      read_anything::mojom::ReadAnythingThemePtr new_theme) override;

  // ReadAnythingCoordinator::Observer:
  void OnCoordinatorDestroyed() override;

 private:
  raw_ptr<ReadAnythingCoordinator> coordinator_;
  raw_ptr<ReadAnythingPageHandler::Delegate> delegate_;

  raw_ptr<Browser> browser_;

  mojo::Receiver<read_anything::mojom::PageHandler> receiver_;
  mojo::Remote<read_anything::mojom::Page> page_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PAGE_HANDLER_H_
