// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_PROMO_BROWSER_COMMAND_PROMO_BROWSER_COMMAND_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_PROMO_BROWSER_COMMAND_PROMO_BROWSER_COMMAND_HANDLER_H_

#include <memory>

#include "chrome/browser/command_updater_delegate.h"
#include "chrome/browser/promo_browser_command/promo_browser_command.mojom.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class CommandUpdater;
class Profile;

// Struct containing the information needed to customize/configure the feedback
// form. Used to populate arguments passed to chrome::ShowFeedbackPage().
struct FeedbackCommandSettings {
  FeedbackCommandSettings() = default;

  FeedbackCommandSettings(const GURL& url,
                          chrome::FeedbackSource source,
                          std::string category)
      : url(url), source(source), category(category) {}

  GURL url;
  chrome::FeedbackSource source = chrome::kFeedbackSourceCount;
  std::string category;
};

// Handles browser commands send from JS.
class PromoBrowserCommandHandler
    : public CommandUpdaterDelegate,
      public promo_browser_command::mojom::CommandHandler {
 public:
  static const char kPromoBrowserCommandHistogramName[];

  PromoBrowserCommandHandler(
      mojo::PendingReceiver<promo_browser_command::mojom::CommandHandler>
          pending_page_handler,
      Profile* profile,
      std::vector<promo_browser_command::mojom::Command> supported_commands);
  ~PromoBrowserCommandHandler() override;

  // promo_browser_command::mojom::CommandHandler:
  void CanExecuteCommand(promo_browser_command::mojom::Command command_id,
                         CanExecuteCommandCallback callback) override;
  void ExecuteCommand(promo_browser_command::mojom::Command command_id,
                      promo_browser_command::mojom::ClickInfoPtr click_info,
                      ExecuteCommandCallback callback) override;

  // CommandUpdaterDelegate:
  void ExecuteCommandWithDisposition(
      int command_id,
      WindowOpenDisposition disposition) override;

  void ConfigureFeedbackCommand(FeedbackCommandSettings settings);

 protected:
  void EnableSupportedCommands();

  virtual CommandUpdater* GetCommandUpdater();

 private:
  virtual void NavigateToURL(const GURL& url,
                             WindowOpenDisposition disposition);
  virtual void OpenFeedbackForm();

  FeedbackCommandSettings feedback_settings_;
  Profile* profile_;
  std::vector<promo_browser_command::mojom::Command> supported_commands_;
  std::unique_ptr<CommandUpdater> command_updater_;
  mojo::Receiver<promo_browser_command::mojom::CommandHandler> page_handler_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_PROMO_BROWSER_COMMAND_PROMO_BROWSER_COMMAND_HANDLER_H_
