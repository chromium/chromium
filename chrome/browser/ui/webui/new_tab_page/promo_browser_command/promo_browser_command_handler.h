// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_PROMO_BROWSER_COMMAND_PROMO_BROWSER_COMMAND_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_PROMO_BROWSER_COMMAND_PROMO_BROWSER_COMMAND_HANDLER_H_

#include <memory>

#include "chrome/browser/command_updater_delegate.h"
#include "chrome/browser/promo_browser_command/promo_browser_command.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/window_open_disposition.h"

class CommandUpdater;
class GURL;
class Profile;

// Handles promo browser commands send from JS.
class PromoBrowserCommandHandler
    : public CommandUpdaterDelegate,
      public promo_browser_command::mojom::CommandHandler {
 public:
  static const char kPromoBrowserCommandHistogramName[];

  PromoBrowserCommandHandler(
      mojo::PendingReceiver<promo_browser_command::mojom::CommandHandler>
          pending_page_handler,
      Profile* profile);
  ~PromoBrowserCommandHandler() override;

  // promo_browser_command::mojom::CommandHandler:
  void CanShowPromoWithCommand(
      promo_browser_command::mojom::Command command_id,
      CanShowPromoWithCommandCallback callback) override;
  void ExecuteCommand(promo_browser_command::mojom::Command command_id,
                      promo_browser_command::mojom::ClickInfoPtr click_info,
                      ExecuteCommandCallback callback) override;

  // CommandUpdaterDelegate:
  void ExecuteCommandWithDisposition(
      int command_id,
      WindowOpenDisposition disposition) override;

 protected:
  void EnableCommands();

  virtual CommandUpdater* GetCommandUpdater();

 private:
  virtual void NavigateToURL(const GURL& url,
                             WindowOpenDisposition disposition);

  Profile* profile_;
  std::unique_ptr<CommandUpdater> command_updater_;
  mojo::Receiver<promo_browser_command::mojom::CommandHandler> page_handler_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_PROMO_BROWSER_COMMAND_PROMO_BROWSER_COMMAND_HANDLER_H_
