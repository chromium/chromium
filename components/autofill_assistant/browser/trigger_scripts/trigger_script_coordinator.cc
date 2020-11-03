// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_scripts/trigger_script_coordinator.h"

namespace autofill_assistant {

// TODO(b/171776026): implement this stub.
TriggerScriptCoordinator::TriggerScriptCoordinator(
    Client* client,
    std::unique_ptr<WebController> web_controller,
    std::unique_ptr<ServiceRequestSender> request_sender,
    const GURL& get_trigger_scripts_server)
    : content::WebContentsObserver(client->GetWebContents()) {}

TriggerScriptCoordinator::~TriggerScriptCoordinator() = default;

void TriggerScriptCoordinator::Start(const GURL& url) {}

void TriggerScriptCoordinator::PerformTriggerScriptAction(
    TriggerScriptProto::TriggerScriptAction action) {}

void TriggerScriptCoordinator::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TriggerScriptCoordinator::RemoveObserver(const Observer* observer) {
  observers_.RemoveObserver(observer);
}

void TriggerScriptCoordinator::OnVisibilityChanged(
    content::Visibility visibility) {}

}  // namespace autofill_assistant
