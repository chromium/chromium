// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/required_components_controller.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/component_updater_switches.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"

namespace component_updater {

RequiredComponentsController::RequiredComponentsController(
    std::vector<std::string> components)
    : components_(std::move(components)) {}

RequiredComponentsController::~RequiredComponentsController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool RequiredComponentsController::IsRequiredComponent(
    const std::string& name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::ranges::any_of(components_, [&name](const auto& pattern) {
    return base::MatchPattern(name, pattern);
  });
}

bool RequiredComponentsController::RequestComponentUpdate(
    const ComponentRegistration& component) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsRequiredComponent(component.name)) {
    return false;
  }

  if (update_start_time_.is_null()) {
    update_start_time_ = base::TimeTicks::Now();
  }

  requested_components_[component.app_id] = component.name;

  ready_ = false;

  return true;
}

void RequiredComponentsController::ToCrxComponent(
    const ComponentRegistration& component,
    update_client::CrxComponent& crx) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsRequiredComponent(component.name)) {
    return;
  }

  // Required componenst are always updated.
  crx.updates_enabled = true;
}

void RequiredComponentsController::OnEvent(
    const update_client::CrxUpdateItem& update_item) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!requested_components_.contains(update_item.id)) {
    return;
  }

  const std::string& name = requested_components_[update_item.id];

  switch (update_item.state) {
    case update_client::ComponentState::kUpToDate:
      LOG(WARNING) << "Component \'" << name << "\' up to date, "
                   << "version: " << update_item.next_version;
      break;
    case update_client::ComponentState::kUpdated:
      LOG(WARNING) << "Component \'" << name << "\' updated, "
                   << "version: " << update_item.next_version;
      break;
    case update_client::ComponentState::kUpdateError:
      LOG(FATAL) << "Component \'" << name << "\' update error, "
                 << "error code: " << update_item.error_code;
    default:
      return;
  }

  ready_components_.insert(name);
  requested_components_.erase(update_item.id);

  if (requested_components_.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&RequiredComponentsController::CheckComponentsReady,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void RequiredComponentsController::EnsureRequiredComponentsReady(
    base::TimeDelta timeout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ready_) {
    return;
  }

  base::RunLoop run_loop;
  ready_callback_ = run_loop.QuitClosure();

  base::OneShotTimer timer;
  timer.Start(FROM_HERE, timeout, run_loop.QuitClosure());

  run_loop.Run();

  if (!ready_) {
    LOG(FATAL)
        << "Timed out waiting for all the required components to update. "
           "Pending components: "
        << GetRequestedComponentNames() << ".";
  }
}

void RequiredComponentsController::CheckComponentsReady() {
  if (!requested_components_.empty() || ready_) {
    return;
  }

  // Some required components may be registered later in the startup sequence
  // (e.g., components chained from a manifest component). If a component is
  // explicitly listed by its full name without wildcards, do not mark
  // required components as ready until that component has been updated.
  if (std::ranges::any_of(components_, [this](const auto& component) {
        return component.find_first_of("*?") == std::string::npos &&
               !ready_components_.contains(component);
      })) {
    return;
  }

  if (!update_start_time_.is_null()) {
    LOG(WARNING) << "Required components updated in "
                 << (base::TimeTicks::Now() - update_start_time_);
    update_start_time_ = base::TimeTicks();
  }

  ready_ = true;
  if (ready_callback_) {
    std::move(ready_callback_).Run();
  }
}

std::string RequiredComponentsController::GetRequestedComponentNames() const {
  base::flat_set<std::string> names;
  for (const auto& [id, name] : requested_components_) {
    names.insert(name);
  }

  // Include explicitly listed required components (without wildcards) that are
  // still pending even if they have not been registered yet (e.g., chained
  // components whose registration was delayed).
  for (const auto& component : components_) {
    if (component.find_first_of("*?") == std::string::npos &&
        !ready_components_.contains(component)) {
      names.insert(component);
    }
  }
  std::string result;
  for (const auto& name : names) {
    if (!result.empty()) {
      result += ", ";
    }
    base::StrAppend(&result, {"\"", name, "\""});
  }
  return result;
}

}  // namespace component_updater
