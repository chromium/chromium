// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_HANDLER_H_

#include "chrome/browser/ui/webui/whats_new/whats_new.mojom.h"
#include "components/user_education/webui/whats_new_registry.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/mojo_web_ui_controller.h"

class Profile;

// Page handler for chrome://whats-new.
class WhatsNewHandler : public whats_new::mojom::PageHandler {
 public:
  WhatsNewHandler(mojo::PendingReceiver<whats_new::mojom::PageHandler> receiver,
                  mojo::PendingRemote<whats_new::mojom::Page> page,
                  Profile* profile,
                  content::WebContents* web_contents,
                  const base::Time& navigation_start_time,
                  const whats_new::WhatsNewRegistry* whats_new_registry);
  ~WhatsNewHandler() override;
  WhatsNewHandler(const WhatsNewHandler&) = delete;
  WhatsNewHandler& operator=(const WhatsNewHandler&) = delete;

 private:
  // whats_new::mojom::PageHandler
  void GetServerUrl(bool is_staging, GetServerUrlCallback callback) override;
  FRIEND_TEST_ALL_PREFIXES(WhatsNewHandlerTest, GetServerUrl);
  FRIEND_TEST_ALL_PREFIXES(WhatsNewHandlerTest, HistogramsAreEmitted);
  FRIEND_TEST_ALL_PREFIXES(WhatsNewHandlerTest, SurveyIsTriggered);
  FRIEND_TEST_ALL_PREFIXES(WhatsNewHandlerTest, SurveyIsTriggeredWithOverride);
  FRIEND_TEST_ALL_PREFIXES(WhatsNewHandlerTest,
                           SurveyIsNotTriggeredForPreviouslyUsedEdition);

  void RecordTimeToLoadContent(base::Time time) override;
  void RecordVersionPageLoaded(bool is_auto_open) override;
  void RecordEditionPageLoaded(const std::string& page_uid,
                               bool is_auto_open) override;
  void RecordModuleImpression(
      const std::string& module_name,
      whats_new::mojom::ModulePosition position) override;
  void RecordExploreMoreToggled(bool expanded) override;
  void RecordScrollDepth(whats_new::mojom::ScrollDepth depth) override;
  void RecordTimeOnPage(base::TimeDelta time) override;
  void RecordModuleLinkClicked(
      const std::string& module_name,
      whats_new::mojom::ModulePosition position) override;
  void RecordModuleVideoStarted(
      const std::string& module_name,
      whats_new::mojom::ModulePosition position) override;
  void RecordModuleVideoEnded(
      const std::string& module_name,
      whats_new::mojom::ModulePosition position) override;
  void RecordModulePlayClicked(
      const std::string& module_name,
      whats_new::mojom::ModulePosition position) override;
  void RecordModulePauseClicked(
      const std::string& module_name,
      whats_new::mojom::ModulePosition position) override;
  void RecordModuleRestartClicked(
      const std::string& module_name,
      whats_new::mojom::ModulePosition position) override;
  void RecordBrowserCommandExecuted() override;

  // Makes a request to show a HaTS survey.
  void TryShowHatsSurveyWithTimeout();

  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  base::Time navigation_start_time_;

  // Reference to the WhatsNewRegistry global singleton. This is
  // required to outlive the WhatsNewHanlder instance.
  const raw_ref<const whats_new::WhatsNewRegistry> whats_new_registry_;

  // These are located at the end of the list of member variables to ensure the
  // WebUI page is disconnected before other members are destroyed.
  mojo::Receiver<whats_new::mojom::PageHandler> receiver_;
  mojo::Remote<whats_new::mojom::Page> page_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_HANDLER_H_
