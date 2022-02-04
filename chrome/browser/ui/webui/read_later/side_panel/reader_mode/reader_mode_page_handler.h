// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_READ_LATER_SIDE_PANEL_READER_MODE_READER_MODE_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_READ_LATER_SIDE_PANEL_READER_MODE_READER_MODE_PAGE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/read_later/side_panel/reader_mode/reader_mode.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ui {
struct AXTreeUpdate;
}

class ReaderModePageHandler : public reader_mode::mojom::PageHandler {
 public:
  explicit ReaderModePageHandler(
      mojo::PendingRemote<reader_mode::mojom::Page> page,
      mojo::PendingReceiver<reader_mode::mojom::PageHandler> receiver);
  ReaderModePageHandler(const ReaderModePageHandler&) = delete;
  ReaderModePageHandler& operator=(const ReaderModePageHandler&) = delete;
  ~ReaderModePageHandler() override;

  // reader_mode::mojom::PageHandler:
  void ShowUI() override;

 private:
  void CombineTextNodesAndMakeCallback(const ui::AXTreeUpdate& update);

  mojo::Receiver<reader_mode::mojom::PageHandler> receiver_;
  mojo::Remote<reader_mode::mojom::Page> page_;
  base::WeakPtrFactory<ReaderModePageHandler> weak_pointer_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_READ_LATER_SIDE_PANEL_READER_MODE_READER_MODE_PAGE_HANDLER_H_
