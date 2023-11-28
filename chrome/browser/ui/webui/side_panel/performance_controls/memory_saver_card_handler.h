// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_PERFORMANCE_CONTROLS_MEMORY_SAVER_CARD_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_PERFORMANCE_CONTROLS_MEMORY_SAVER_CARD_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/side_panel/performance_controls/performance.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class PerformanceSidePanelUI;

class MemorySaverCardHandler
    : public side_panel::mojom::MemorySaverCardHandler {
 public:
  explicit MemorySaverCardHandler(
      mojo::PendingReceiver<side_panel::mojom::MemorySaverCardHandler> receiver,
      mojo::PendingRemote<side_panel::mojom::MemorySaverCard> memory_saver_card,
      PerformanceSidePanelUI* performance_ui);
  MemorySaverCardHandler(const MemorySaverCardHandler&) = delete;
  MemorySaverCardHandler& operator=(const MemorySaverCardHandler&) = delete;
  ~MemorySaverCardHandler() override;

 private:
  mojo::Receiver<side_panel::mojom::MemorySaverCardHandler> receiver_;
  mojo::Remote<side_panel::mojom::MemorySaverCard> memory_saver_card_;
  raw_ptr<PerformanceSidePanelUI> performance_ui_ = nullptr;
  raw_ptr<Profile> profile_;
};

#endif
