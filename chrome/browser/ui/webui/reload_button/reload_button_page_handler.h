// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_RELOAD_BUTTON_RELOAD_BUTTON_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_RELOAD_BUTTON_RELOAD_BUTTON_PAGE_HANDLER_H_

#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/webui/reload_button/reload_button.mojom.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class ReloadButtonPageHandler : public reload_button::mojom::PageHandler {
 public:
  ReloadButtonPageHandler(
      mojo::PendingReceiver<reload_button::mojom::PageHandler> receiver,
      mojo::PendingRemote<reload_button::mojom::Page> page,
      content::WebContents* web_contents);

  ReloadButtonPageHandler(const ReloadButtonPageHandler&) = delete;
  ReloadButtonPageHandler& operator=(const ReloadButtonPageHandler&) = delete;

  ~ReloadButtonPageHandler() override;

  void SetLoadingState(bool is_loading, bool force);

  // reload_button::mojom::PageHandler:
  void Reload() override;
  void StopReload() override;

 private:
  mojo::Receiver<reload_button::mojom::PageHandler> receiver_;
  mojo::Remote<reload_button::mojom::Page> page_;
  raw_ptr<CommandUpdater> command_updater_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_RELOAD_BUTTON_RELOAD_BUTTON_PAGE_HANDLER_H_
