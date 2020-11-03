// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_TRIGGER_SCRIPT_COORDINATOR_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_TRIGGER_SCRIPT_COORDINATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/service_request_sender.h"
#include "components/autofill_assistant/browser/trigger_scripts/trigger_script.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace autofill_assistant {

// Fetches and coordinates trigger scripts for a specific url. Similar in scope
// and responsibility to a regular |controller|, but for trigger scripts instead
// of regular scripts.
class TriggerScriptCoordinator : public content::WebContentsObserver {
 public:
  // Observer interface for listeners interested in status updates of this
  // coordinator.
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    ~Observer() override = default;

    virtual void OnTriggerScriptShown(const TriggerScriptUIProto& proto) = 0;
    virtual void OnTriggerScriptHidden() = 0;
    virtual void OnTriggerScriptFinished(int state) = 0;
  };

  // |client| and |web_contents| must outlive this instance.
  TriggerScriptCoordinator(Client* client,
                           std::unique_ptr<WebController> web_controller,
                           std::unique_ptr<ServiceRequestSender> request_sender,
                           const GURL& get_trigger_scripts_server);
  ~TriggerScriptCoordinator() override;
  TriggerScriptCoordinator(const TriggerScriptCoordinator&) = delete;
  TriggerScriptCoordinator& operator=(const TriggerScriptCoordinator&) = delete;

  // Retrieves all trigger scripts for |url| and starts evaluating their
  // preconditions. Observers will be notified of all relevant status updates.
  void Start(const GURL& url);

  // Performs |action|. This is usually invoked by the UI as a result of user
  // interactions.
  void PerformTriggerScriptAction(
      TriggerScriptProto::TriggerScriptAction action);

  void AddObserver(Observer* observer);
  void RemoveObserver(const Observer* observer);

 private:
  struct PendingTriggerScript {
    TriggerScript trigger_script;
    bool waiting_for_precondition_no_longer_true = false;
  };

  // From content::WebContentsObserver.
  void OnVisibilityChanged(content::Visibility visibility) override;

  // The list of currently registered observers.
  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<TriggerScriptCoordinator> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_TRIGGER_SCRIPT_COORDINATOR_H_
