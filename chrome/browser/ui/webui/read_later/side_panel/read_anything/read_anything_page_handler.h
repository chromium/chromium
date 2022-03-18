// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_READ_LATER_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_READ_LATER_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PAGE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/read_later/side_panel/read_anything/read_anything.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ui {
struct AXTreeUpdate;
}

class ReadAnythingPageHandler : public read_anything::mojom::PageHandler {
 public:
  explicit ReadAnythingPageHandler(
      mojo::PendingRemote<read_anything::mojom::Page> page,
      mojo::PendingReceiver<read_anything::mojom::PageHandler> receiver);
  ReadAnythingPageHandler(const ReadAnythingPageHandler&) = delete;
  ReadAnythingPageHandler& operator=(const ReadAnythingPageHandler&) = delete;
  ~ReadAnythingPageHandler() override;

  // read_anything::mojom::PageHandler:
  void ShowUI() override;

 private:
  void CombineTextNodesAndMakeCallback(const ui::AXTreeUpdate& update);

  mojo::Receiver<read_anything::mojom::PageHandler> receiver_;
  mojo::Remote<read_anything::mojom::Page> page_;
  base::WeakPtrFactory<ReadAnythingPageHandler> weak_pointer_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_READ_LATER_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PAGE_HANDLER_H_
