// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_engine.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/uuid.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/component.h"
#include "components/update_client/configurator.h"
#include "components/update_client/crx_cache.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/features.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/protocol_parser.h"
#include "components/update_client/update_checker.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace update_client {

UpdateContext::UpdateContext(
    scoped_refptr<Configurator> config,
    absl::optional<scoped_refptr<CrxCache>> crx_cache,
    bool is_foreground,
    bool is_install,
    const std::vector<std::string>& ids,
    UpdateClient::CrxStateChangeCallback crx_state_change_callback,
    const UpdateEngine::NotifyObserversCallback& notify_observers_callback,
    UpdateEngine::Callback callback,
    PersistedData* persisted_data,
    bool is_update_check_only)
    : config(config),
      crx_cache_(crx_cache),
      is_foreground(is_foreground),
      is_install(is_install),
      ids(ids),
      crx_state_change_callback(crx_state_change_callback),
      notify_observers_callback(notify_observers_callback),
      callback(std::move(callback)),
      session_id(base::StrCat(
          {"{", base::Uuid::GenerateRandomV4().AsLowercaseString(), "}"})),
      persisted_data(persisted_data),
      is_update_check_only(is_update_check_only) {
  for (const auto& id : ids) {
    components.insert(
        std::make_pair(id, std::make_unique<Component>(*this, id)));
  }
}

UpdateContext::~UpdateContext() = default;

UpdateEngine::UpdateEngine(
    scoped_refptr<Configurator> config,
    UpdateChecker::Factory update_checker_factory,
    scoped_refptr<PingManager> ping_manager,
    const NotifyObserversCallback& notify_observers_callback)
    : config_(config),
      update_checker_factory_(update_checker_factory),
      ping_manager_(ping_manager),
      metadata_(
          std::make_unique<PersistedData>(config->GetPrefService(),
                                          config->GetActivityDataService())),
      notify_observers_callback_(notify_observers_callback) {
  absl::optional<base::FilePath> crx_cache_path = config->GetCrxCachePath();
  if (base::FeatureList::IsEnabled(features::kPuffinPatches) &&
      crx_cache_path.has_value()) {
    CrxCache::Options options(crx_cache_path.value());
    crx_cache_ = absl::optional<scoped_refptr<CrxCache>>(
        base::MakeRefCounted<CrxCache>(options));
  } else {
    crx_cache_ = absl::nullopt;
  }
}

UpdateEngine::~UpdateEngine() = default;

base::RepeatingClosure UpdateEngine::Update(
    bool is_foreground,
    bool is_install,
    const std::vector<std::string>& ids,
    UpdateClient::CrxDataCallback crx_data_callback,
    UpdateClient::CrxStateChangeCallback crx_state_change_callback,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return InvokeOperation(is_foreground, /*is_update_check_only=*/false,
                         is_install, ids, std::move(crx_data_callback),
                         std::move(crx_state_change_callback),
                         std::move(callback));
}

void UpdateEngine::CheckForUpdate(
    bool is_foreground,
    const std::string& id,
    UpdateClient::CrxDataCallback crx_data_callback,
    UpdateClient::CrxStateChangeCallback crx_state_change_callback,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  InvokeOperation(is_foreground, /*is_update_check_only=*/true,
                  /*is_install=*/false, {id}, std::move(crx_data_callback),
                  std::move(crx_state_change_callback), std::move(callback));
}

base::RepeatingClosure UpdateEngine::InvokeOperation(
    bool is_foreground,
    bool is_update_check_only,
    bool is_install,
    const std::vector<std::string>& ids,
    UpdateClient::CrxDataCallback crx_data_callback,
    UpdateClient::CrxStateChangeCallback crx_state_change_callback,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ids.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), Error::INVALID_ARGUMENT));
    return base::DoNothing();
  }

  if (IsThrottled(is_foreground)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), Error::RETRY_LATER));
    return base::DoNothing();
  }

  scoped_refptr<UpdateContext> update_context =
      base::MakeRefCounted<UpdateContext>(
          config_, crx_cache_, is_foreground, is_install, ids,
          crx_state_change_callback, notify_observers_callback_,
          std::move(callback), metadata_.get(), is_update_check_only);
  CHECK(!update_context->session_id.empty());

  const auto result = update_contexts_.insert(
      std::make_pair(update_context->session_id, update_context));
  CHECK(result.second);

  // Calls out to get the corresponding CrxComponent data for the components.
  std::move(crx_data_callback)
      .Run(ids,
           base::BindOnce(&UpdateEngine::StartOperation, this, update_context));
  return is_update_check_only
             ? base::DoNothing()
             : base::BindRepeating(
                   [](scoped_refptr<UpdateContext> context) {
                     context->is_cancelled = true;
                     for (const auto& entry : context->components) {
                       entry.second->Cancel();
                     }
                   },
                   update_context);
}

void UpdateEngine::StartOperation(
    scoped_refptr<UpdateContext> update_context,
    const std::vector<absl::optional<CrxComponent>>& crx_components) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (crx_components.size() != update_context->ids.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(update_context->callback),
                                  Error::BAD_CRX_DATA_CALLBACK));
    return;
  }

  for (size_t i = 0; i != update_context->ids.size(); ++i) {
    const auto& id = update_context->ids[i];

    CHECK_EQ(update_context->components[id]->state(), ComponentState::kNew);

    const auto crx_component = crx_components[i];
    if (crx_component && (id == GetCrxComponentID(*crx_component))) {
      // This component can be checked for updates.
      auto& component = update_context->components[id];
      component->set_crx_component(*crx_component);
      component->set_previous_version(component->crx_component()->version);
      component->set_previous_fp(component->crx_component()->fingerprint);
      update_context->components_to_check_for_updates.push_back(id);
    } else {
      // |CrxDataCallback| did not return a CrxComponent instance for this
      // component, which most likely, has been uninstalled. This component
      // is going to be transitioned to an error state when the its |Handle|
      // method is called later on by the engine.
      update_context->component_queue.push(id);
    }
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(update_context->components_to_check_for_updates.empty()
                         ? &UpdateEngine::HandleComponent
                         : &UpdateEngine::DoUpdateCheck,
                     this, update_context));
}

void UpdateEngine::DoUpdateCheck(scoped_refptr<UpdateContext> update_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(update_context);

  // Make the components transition from |kNew| to |kChecking| state.
  for (const auto& id : update_context->components_to_check_for_updates)
    update_context->components[id]->Handle(base::DoNothing());

  update_context->update_checker =
      update_checker_factory_.Run(config_, metadata_.get());

  update_context->update_checker->CheckForUpdates(
      update_context, config_->ExtraRequestParams(),
      base::BindOnce(&UpdateEngine::UpdateCheckResultsAvailable, this,
                     update_context));
}

void UpdateEngine::UpdateCheckResultsAvailable(
    scoped_refptr<UpdateContext> update_context,
    const absl::optional<ProtocolParser::Results>& results,
    ErrorCategory error_category,
    int error,
    int retry_after_sec) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(update_context);

  update_context->retry_after_sec = retry_after_sec;

  // Only positive values for throttle_sec are effective. 0 means that no
  // throttling occurs and it resets the throttle.
  // Negative values are not trusted and are ignored.
  constexpr int kMaxRetryAfterSec = 24 * 60 * 60;  // 24 hours.
  const int throttle_sec =
      std::min(update_context->retry_after_sec, kMaxRetryAfterSec);
  if (throttle_sec >= 0) {
    PersistedData(config_->GetPrefService(), nullptr)
        .SetThrottleUpdatesUntil(throttle_sec ? base::Time::Now() +
                                                    base::Seconds(throttle_sec)
                                              : base::Time());
  }

  update_context->update_check_error = error;

  if (error) {
    CHECK(!results);
    for (const auto& id : update_context->components_to_check_for_updates) {
      CHECK_EQ(1u, update_context->components.count(id));
      auto& component = update_context->components.at(id);
      component->SetUpdateCheckResult(absl::nullopt,
                                      ErrorCategory::kUpdateCheck, error);
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&UpdateEngine::UpdateCheckComplete, this,
                                  update_context));
    return;
  }

  CHECK(results);
  CHECK_EQ(0, error);

  std::map<std::string, ProtocolParser::Result> id_to_result;
  for (const auto& result : results->list)
    id_to_result[result.extension_id] = result;

  for (const auto& id : update_context->components_to_check_for_updates) {
    CHECK_EQ(1u, update_context->components.count(id));
    auto& component = update_context->components.at(id);
    const auto& it = id_to_result.find(id);
    if (it != id_to_result.end()) {
      const auto result = it->second;
      const auto pair = [](const std::string& status) {
        // First, handle app status literals which can be folded down as an
        // updatecheck status
        if (status == "error-unknownApplication")
          return std::make_pair(ErrorCategory::kUpdateCheck,
                                ProtocolError::UNKNOWN_APPLICATION);
        if (status == "restricted")
          return std::make_pair(ErrorCategory::kUpdateCheck,
                                ProtocolError::RESTRICTED_APPLICATION);
        if (status == "error-invalidAppId")
          return std::make_pair(ErrorCategory::kUpdateCheck,
                                ProtocolError::INVALID_APPID);
        if (status == "error-osnotsupported") {
          return std::make_pair(ErrorCategory::kUpdateCheck,
                                ProtocolError::OS_NOT_SUPPORTED);
        }
        if (status == "error-hwnotsupported") {
          return std::make_pair(ErrorCategory::kUpdateCheck,
                                ProtocolError::HW_NOT_SUPPORTED);
        }
        if (status == "error-hash") {
          return std::make_pair(ErrorCategory::kUpdateCheck,
                                ProtocolError::NO_HASH);
        }
        if (status == "error-unsupportedprotocol") {
          return std::make_pair(ErrorCategory::kUpdateCheck,
                                ProtocolError::UNSUPPORTED_PROTOCOL);
        }
        if (status == "error-internal") {
          return std::make_pair(ErrorCategory::kUpdateCheck,
                                ProtocolError::INTERNAL);
        }
        // If the parser has return a valid result and the status is not one of
        // the literals above, then this must be a success an not a parse error.
        return std::make_pair(ErrorCategory::kNone, ProtocolError::NONE);
      }(result.status);
      component->SetUpdateCheckResult(result, pair.first,
                                      static_cast<int>(pair.second));
    } else {
      component->SetUpdateCheckResult(
          absl::nullopt, ErrorCategory::kUpdateCheck,
          static_cast<int>(ProtocolError::UPDATE_RESPONSE_NOT_FOUND));
    }
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&UpdateEngine::UpdateCheckComplete, this, update_context));
}

void UpdateEngine::UpdateCheckComplete(
    scoped_refptr<UpdateContext> update_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(update_context);

  for (const auto& id : update_context->components_to_check_for_updates) {
    update_context->component_queue.push(id);

    // Handle the |kChecking| state and transition the component to the
    // next state, depending on the update check results.
    CHECK_EQ(1u, update_context->components.count(id));
    auto& component = update_context->components.at(id);
    CHECK_EQ(component->state(), ComponentState::kChecking);
    component->Handle(base::DoNothing());
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&UpdateEngine::HandleComponent, this, update_context));
}

void UpdateEngine::HandleComponent(
    scoped_refptr<UpdateContext> update_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(update_context);

  auto& queue = update_context->component_queue;

  if (queue.empty()) {
    const Error error = update_context->update_check_error
                            ? Error::UPDATE_CHECK_ERROR
                            : Error::NONE;

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&UpdateEngine::UpdateComplete, this,
                                  update_context, error));
    return;
  }

  const auto& id = queue.front();
  CHECK_EQ(1u, update_context->components.count(id));
  const auto& component = update_context->components.at(id);
  CHECK(component);

  auto& next_update_delay = update_context->next_update_delay;
  if (!next_update_delay.is_zero() && component->IsUpdateAvailable()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&UpdateEngine::HandleComponent, this, update_context),
        next_update_delay);
    next_update_delay = base::TimeDelta();
    component->NotifyWait();
    return;
  }

  component->Handle(base::BindOnce(&UpdateEngine::HandleComponentComplete, this,
                                   update_context));
}

void UpdateEngine::HandleComponentComplete(
    scoped_refptr<UpdateContext> update_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(update_context);

  auto& queue = update_context->component_queue;
  CHECK(!queue.empty());

  const auto& id = queue.front();
  CHECK_EQ(1u, update_context->components.count(id));
  const auto& component = update_context->components.at(id);
  CHECK(component);

  base::OnceClosure callback =
      base::BindOnce(&UpdateEngine::HandleComponent, this, update_context);
  if (component->IsHandled()) {
    update_context->next_update_delay = component->GetUpdateDuration();
    queue.pop();
    if (!component->events().empty()) {
      ping_manager_->SendPing(
          *component, *metadata_,
          base::BindOnce([](base::OnceClosure callback, int,
                            const std::string&) { std::move(callback).Run(); },
                         std::move(callback)));
      return;
    }
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

void UpdateEngine::UpdateComplete(scoped_refptr<UpdateContext> update_context,
                                  Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(update_context);

  const auto num_erased = update_contexts_.erase(update_context->session_id);
  CHECK_EQ(1u, num_erased);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(update_context->callback), error));
}

bool UpdateEngine::GetUpdateState(const std::string& id,
                                  CrxUpdateItem* update_item) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& context : update_contexts_) {
    const auto& components = context.second->components;
    const auto it = components.find(id);
    if (it != components.end()) {
      *update_item = it->second->GetCrxUpdateItem();
      return true;
    }
  }
  return false;
}

bool UpdateEngine::IsThrottled(bool is_foreground) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Time throttle_updates_until =
      PersistedData(config_->GetPrefService(), nullptr)
          .GetThrottleUpdatesUntil();

  if (is_foreground || throttle_updates_until.is_null()) {
    return false;
  }

  const auto now(base::Time::Now());

  // Throttle the calls in the interval (t - 1 day, t) to limit the effect of
  // unset clocks or clock drift.
  return throttle_updates_until - base::Days(1) < now &&
         now < throttle_updates_until;
}

void UpdateEngine::SendPing(const CrxComponent& crx_component,
                            int event_type,
                            int result_code,
                            int error_code,
                            int extra_code1,
                            Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const std::string& id = crx_component.app_id;

  const auto update_context = base::MakeRefCounted<UpdateContext>(
      config_, crx_cache_, false, false, std::vector<std::string>{id},
      UpdateClient::CrxStateChangeCallback(),
      UpdateEngine::NotifyObserversCallback(), std::move(callback),
      metadata_.get(), /*is_update_check_only=*/false);
  CHECK(!update_context->session_id.empty());

  const auto result = update_contexts_.insert(
      std::make_pair(update_context->session_id, update_context));
  CHECK(result.second);

  CHECK(update_context);
  CHECK_EQ(1u, update_context->ids.size());
  CHECK_EQ(1u, update_context->components.count(id));
  const auto& component = update_context->components.at(id);

  component->PingOnly(crx_component, event_type, result_code, error_code,
                      extra_code1);

  update_context->component_queue.push(id);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&UpdateEngine::HandleComponent, this, update_context));
}

}  // namespace update_client
