// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ACTOR_ACTOR_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ACTOR_ACTOR_AUTOFILL_MANAGER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ref.h"
#include "components/actor/core/task_id.h"
#include "components/autofill/core/browser/actor/actor_key_metrics_recorder.h"

namespace autofill {

class AutofillClient;

// Manages all actor-related autofill state and observers for a tab.
class ActorAutofillManager {
 public:
  explicit ActorAutofillManager(AutofillClient* client);
  ActorAutofillManager(const ActorAutofillManager&) = delete;
  ActorAutofillManager& operator=(const ActorAutofillManager&) = delete;
  ~ActorAutofillManager();

  // Returns whether the tab is in Actor mode.
  bool IsTabInActorMode() const;

  // The active actor task ID, if any.
  std::optional<::actor::TaskId> active_actor_task() const {
    return active_actor_task_;
  }
  void set_active_actor_task(std::optional<::actor::TaskId> task_id) {
    active_actor_task_ = std::move(task_id);
  }

  ActorKeyMetricsRecorder& key_metrics_recorder() {
    return key_metrics_recorder_;
  }

 private:
  // Responsible for keeping track if (and which) actor is interacting with
  // the current tab. When present, some parts of Autofill may behave
  // differently. There can be at most one actor on a given tab. If there is no
  // actor interacting with the current tab it is `std::nullopt`.
  std::optional<::actor::TaskId> active_actor_task_;

  ActorKeyMetricsRecorder key_metrics_recorder_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ACTOR_ACTOR_AUTOFILL_MANAGER_H_
