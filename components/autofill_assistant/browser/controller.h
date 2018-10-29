// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/script_tracker.h"
#include "components/autofill_assistant/browser/service.h"
#include "components/autofill_assistant/browser/ui_delegate.h"
#include "components/autofill_assistant/browser/web_controller.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace autofill_assistant {
class ControllerTest;

// Autofill assistant controller controls autofill assistant action detection,
// display, execution and so on. The instance of this object self deletes when
// the web contents is being destroyed.
class Controller : public ScriptExecutorDelegate,
                   public UiDelegate,
                   public ScriptTracker::Listener,
                   private content::WebContentsObserver,
                   private content::WebContentsDelegate {
 public:
  static void CreateAndStartForWebContents(
      content::WebContents* web_contents,
      std::unique_ptr<Client> client,
      std::unique_ptr<std::map<std::string, std::string>> parameters,
      const GURL& initialUrl);

  // Overrides ScriptExecutorDelegate:
  Service* GetService() override;
  UiController* GetUiController() override;
  WebController* GetWebController() override;
  ClientMemory* GetClientMemory() override;
  const std::map<std::string, std::string>& GetParameters() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  content::WebContents* GetWebContents() override;

 private:
  friend ControllerTest;

  Controller(content::WebContents* web_contents,
             std::unique_ptr<Client> client,
             std::unique_ptr<WebController> web_controller,
             std::unique_ptr<Service> service,
             std::unique_ptr<std::map<std::string, std::string>> parameters,
             const GURL& initialUrl);
  ~Controller() override;

  void GetOrCheckScripts(const GURL& url);
  void OnGetScripts(const GURL& url, bool result, const std::string& response);
  void OnScriptChosen(const std::string& script_path);
  void OnScriptExecuted(const std::string& script_path,
                        ScriptExecutor::Result result);

  // Check script preconditions every few seconds for a certain number of times.
  // If checks are already running, StartPeriodicScriptChecks resets the count.
  //
  // TODO(crbug.com/806868): Find a better solution. This is a brute-force
  // solution that reacts slowly to changes.
  void StartPeriodicScriptChecks();
  void StopPeriodicScriptChecks();
  void OnPeriodicScriptCheck();

  // Overrides content::UiDelegate:
  void OnClickOverlay() override;
  void OnDestroy() override;
  void OnScriptSelected(const std::string& script_path) override;

  // Overrides ScriptTracker::Listener:
  void OnRunnableScriptsChanged(
      const std::vector<ScriptHandle>& runnable_scripts) override;

  // Overrides content::WebContentsObserver:
  void DidGetUserInteraction(const blink::WebInputEvent::Type type) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void WebContentsDestroyed() override;
  void DocumentAvailableInMainFrame() override;

  // Overrides content::WebContentsDelegate:
  void LoadProgressChanged(content::WebContents* source,
                           double progress) override;

  std::unique_ptr<Client> client_;
  std::unique_ptr<WebController> web_controller_;
  std::unique_ptr<Service> service_;
  std::unique_ptr<ScriptTracker> script_tracker_;
  std::unique_ptr<std::map<std::string, std::string>> parameters_;

  // Domain of the last URL the controller requested scripts from.
  std::string script_domain_;
  std::unique_ptr<ClientMemory> memory_;
  bool allow_autostart_ = true;

  // Whether a task for periodic checks is scheduled.
  bool periodic_script_check_scheduled_ = false;

  // Number of remaining periodic checks.
  int periodic_script_check_count_ = 0;
  int total_script_check_count_ = 0;

  // Whether to clear the web_contents delegate when the controller is
  // destroyed.
  bool clear_web_contents_delegate_ = false;

  // Whether we should hide the overlay and show an error message after a first
  // unsuccessful round of preconditions checking.
  bool should_fail_after_checking_scripts_ = false;

  base::WeakPtrFactory<Controller> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Controller);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CONTROLLER_H_
