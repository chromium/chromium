// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSER_COMMAND_BROWSER_COMMAND_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSER_COMMAND_BROWSER_COMMAND_HANDLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/command_updater_delegate.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/ui/user_education/start_tutorial_in_page.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/window_open_disposition.h"
#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"
#include "url/gurl.h"

class CommandUpdater;
class Profile;

namespace user_education {
class TutorialService;
}

// Struct containing the information needed to customize/configure the feedback
// form. Used to populate arguments passed to chrome::ShowFeedbackPage().
struct FeedbackCommandSettings {
  FeedbackCommandSettings() = default;

  FeedbackCommandSettings(const GURL& url,
                          feedback::FeedbackSource source,
                          std::string category)
      : url(url), source(source), category(category) {}

  GURL url;
  feedback::FeedbackSource source = feedback::kFeedbackSourceCount;
  std::string category;
};

// Handles browser commands send from JS.
class BrowserCommandHandler : public CommandUpdaterDelegate,
                              public browser_command::mojom::CommandHandler {
 public:
  static const char kPromoBrowserCommandHistogramName[];

  BrowserCommandHandler(
      mojo::PendingReceiver<browser_command::mojom::CommandHandler>
          pending_page_handler,
      Profile* profile,
      std::vector<browser_command::mojom::Command> supported_commands);
  ~BrowserCommandHandler() override;

  // browser_command::mojom::CommandHandler:
  void CanExecuteCommand(browser_command::mojom::Command command_id,
                         CanExecuteCommandCallback callback) override;
  void ExecuteCommand(browser_command::mojom::Command command_id,
                      browser_command::mojom::ClickInfoPtr click_info,
                      ExecuteCommandCallback callback) override;

  // CommandUpdaterDelegate:
  void ExecuteCommandWithDisposition(
      int command_id,
      WindowOpenDisposition disposition) override;

  void ConfigureFeedbackCommand(FeedbackCommandSettings settings);

 protected:
  void EnableSupportedCommands();

  virtual CommandUpdater* GetCommandUpdater();

  virtual bool BrowserSupportsTabGroups();
  virtual bool DefaultSearchProviderIsGoogle();
  virtual bool BrowserSupportsSavedTabGroups();

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserCommandHandlerTest,
                           StartPasswordManagerTutorialCommand);
  FRIEND_TEST_ALL_PREFIXES(BrowserCommandHandlerTest,
                           StartSavedTabGroupTutorialCommand);

  virtual void NavigateToURL(const GURL& url,
                             WindowOpenDisposition disposition);
  virtual void OpenFeedbackForm();
  virtual void OnTutorialStarted(
      user_education::TutorialIdentifier tutorial_id,
      user_education::TutorialService* tutorial_service);
  virtual void StartTutorial(StartTutorialInPage::Params params);
  virtual bool TutorialServiceExists();
  virtual void NavigateToEnhancedProtectionSetting();
  virtual void OpenPasswordManager();
  virtual void OpenAISettings();
  void StartTabGroupTutorial();
  void OpenNTPAndStartCustomizeChromeTutorial();
  void StartPasswordManagerTutorial();
  void StartSavedTabGroupTutorial();

  FeedbackCommandSettings feedback_settings_;
  raw_ptr<Profile, DanglingUntriaged> profile_;
  std::vector<browser_command::mojom::Command> supported_commands_;
  std::unique_ptr<CommandUpdater> command_updater_;
  mojo::Receiver<browser_command::mojom::CommandHandler> page_handler_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSER_COMMAND_BROWSER_COMMAND_HANDLER_H_
