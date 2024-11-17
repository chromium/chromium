// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/component.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/update_client/action_runner.h"
#include "components/update_client/configurator.h"
#include "components/update_client/network.h"
#include "components/update_client/op_download.h"
#include "components/update_client/op_install.h"
#include "components/update_client/op_puffin.h"
#include "components/update_client/patcher.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/pipeline.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/protocol_serializer.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/unpacker.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/update_client_metrics.h"
#include "components/update_client/update_engine.h"
#include "components/update_client/utils.h"

namespace update_client {

Component::Component(const UpdateContext& update_context, const std::string& id)
    : id_(id),
      state_(std::make_unique<StateNew>(this)),
      update_context_(update_context) {
  // TODO(crbug.com/345250525) - remove when the bug is fixed. We are
  // seeing dumps where the app id is empty in the state change
  // callbacks. This code verifies the invariant that the component
  // instance always has an id.
  if (id_.empty()) {
    DEBUG_ALIAS_FOR_CSTR(dbg_id, id_.c_str(), 64);
    base::debug::DumpWithoutCrashing();
  }
}

Component::~Component() = default;

scoped_refptr<Configurator> Component::config() const {
  return update_context_->config;
}

std::string Component::session_id() const {
  return update_context_->session_id;
}

bool Component::is_foreground() const {
  return update_context_->is_foreground;
}

void Component::Handle(CallbackHandleComplete callback_handle_complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(state_);

  callback_handle_complete_ = std::move(callback_handle_complete);

  state_->Handle(
      base::BindOnce(&Component::ChangeState, base::Unretained(this)));
}

void Component::ChangeState(std::unique_ptr<State> next_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (next_state) {
    state_ = std::move(next_state);
  } else {
    is_handled_ = true;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback_handle_complete_));
}

CrxUpdateItem Component::GetCrxUpdateItem() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/345250525) - remove when the bug is fixed. We are seeing
  // dumps where the app id is empty in the state change callbacks. This code
  // verifies the invariant that the id is always valid.
  if (id_.empty()) {
    DEBUG_ALIAS_FOR_CSTR(dbg_id, id_.c_str(), 64);
    base::debug::DumpWithoutCrashing();
  }

  CrxUpdateItem crx_update_item;
  crx_update_item.state = state_->state();
  if (crx_update_item.state == ComponentState::kUpdating &&
      state_hint_ != ComponentState::kNew) {
    // TODO(crbug.com/353249967): Move state_hint_ into
    // Component::StateUpdating. Component::StateUpdating aggregates three
    // historical substates: kDownloading, kUpdating, and kRun. Callers may be
    // sensitive to which substate the pipeline is in.
    crx_update_item.state = state_hint_;
  }
  crx_update_item.id = id_;
  if (crx_component_) {
    crx_update_item.component = *crx_component_;
  }
  crx_update_item.last_check = last_check_;
  crx_update_item.next_version = next_version_;
  crx_update_item.next_fp = next_fp_;
  crx_update_item.downloaded_bytes = downloaded_bytes_;
  crx_update_item.install_progress = install_progress_;
  crx_update_item.total_bytes = total_bytes_;
  crx_update_item.error_category = error_category_;
  crx_update_item.error_code = error_code_;
  crx_update_item.extra_code1 = extra_code1_;
  crx_update_item.custom_updatecheck_data = custom_attrs_;
  crx_update_item.installer_result = installer_result_;

  return crx_update_item;
}

void Component::SetUpdateCheckResult(
    std::optional<ProtocolParser::Result> result,
    ErrorCategory error_category,
    int error,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(ComponentState::kChecking, state());

  error_category_ = error_category;
  error_code_ = error;

  if (result) {
    CHECK(crx_component_);
    custom_attrs_ = result->custom_attributes;
    if (!result->manifest.packages.empty()) {
      next_version_ = base::Version(result->manifest.version);
      next_fp_ = result->manifest.packages.front().fingerprint;
    } else {
      // When the updatecheck response doesn't contain any packages, use the
      // current version and fingerprint as the "next" version and fingerprint
      // for any events emitted (such as a RunAction event).
      next_version_ = crx_component_->version;
      next_fp_ = crx_component_->fingerprint;
    }
    MakePipeline(
        update_context_->config, update_context_->get_available_space,
        update_context_->is_foreground, update_context_->session_id,
        update_context_->crx_cache_, crx_component_->crx_format_requirement,
        crx_component_->app_id, crx_component_->pk_hash,
        crx_component_->install_data_index, crx_component_->fingerprint,
        crx_component_->installer,
        base::BindRepeating(
            [](base::raw_ref<Component> component, ComponentState state) {
              component->state_hint_ = state;
            },
            base::raw_ref(*this)),
        base::BindRepeating(&Component::AppendEvent, base::Unretained(this)),
        base::BindRepeating(
            [](base::raw_ref<Component> component, int64_t downloaded_bytes,
               int64_t total_bytes) {
              component->downloaded_bytes_ = downloaded_bytes;
              component->total_bytes_ = total_bytes;
              component->NotifyObservers();
            },
            base::raw_ref(*this)),
        base::BindRepeating(
            [](base::raw_ref<Component> component, int progress) {
              if (progress >= 0 && progress <= 100) {
                component->install_progress_ = progress;
              }
              component->NotifyObservers();
            },
            base::raw_ref(*this)),
        base::BindRepeating(
            [](base::raw_ref<Component> component,
               const CrxInstaller::Result& result) {
              component->installer_result_ = result;
              component->error_category_ = result.result.category_;
              component->error_code_ = result.result.code_;
              component->extra_code1_ = result.result.extra_;
            },
            base::raw_ref(*this)),
        crx_component_->action_handler,
        base::BindRepeating(
            [](base::raw_ref<Component> component,
               const CategorizedError& result) {
              component->diff_error_category_ = result.category_;
              component->diff_error_code_ = result.code_;
              component->diff_extra_code1_ = result.extra_;
            },
            base::raw_ref(*this)),
        result.value(),
        base::BindOnce(
            base::BindOnce(
                [](base::raw_ref<Component> component,
                   base::expected<
                       base::OnceCallback<base::OnceClosure(
                           base::OnceCallback<void(const CategorizedError&)>)>,
                       CategorizedError> pipeline) {
                  component->pipeline_ = std::move(pipeline);
                  return true;
                },
                base::raw_ref(*this)))
            .Then(std::move(callback)));
  } else {
    pipeline_ = base::unexpected(
        CategorizedError({.category_ = error_category, .code_ = error}));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
  }
}

bool Component::HasDiffUpdate() const {
  return !crx_diffurls().empty();
}

void Component::AppendEvent(base::Value::Dict event) {
  if (previous_version().IsValid()) {
    event.Set("previousversion", previous_version().GetString());
  }
  if (next_version().IsValid()) {
    event.Set("nextversion", next_version().GetString());
  }
  events_.push_back(std::move(event));
}

void Component::NotifyObservers() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  update_context_->crx_state_change_callback.Run(GetCrxUpdateItem());
}

base::TimeDelta Component::GetUpdateDuration() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (update_begin_.is_null()) {
    return base::TimeDelta();
  }
  const base::TimeDelta update_cost(base::TimeTicks::Now() - update_begin_);
  if (update_cost.is_negative()) {
    return base::TimeDelta();
  }
  return std::min(update_cost, update_context_->config->UpdateDelay());
}

base::Value::Dict Component::MakeEventUpdateComplete() const {
  base::Value::Dict event;
  event.Set("eventtype", update_context_->is_install
                             ? protocol_request::kEventInstall
                             : protocol_request::kEventUpdate);
  event.Set("eventresult",
            static_cast<int>(state() == ComponentState::kUpdated));
  if (error_category() != ErrorCategory::kNone) {
    event.Set("errorcat", static_cast<int>(error_category()));
  }
  if (error_code()) {
    event.Set("errorcode", error_code());
  }
  if (extra_code1()) {
    event.Set("extracode1", extra_code1());
  }
  if (HasDiffUpdate()) {
    const int diffresult = static_cast<int>(!diff_update_failed());
    event.Set("diffresult", diffresult);
  }
  if (diff_error_category() != ErrorCategory::kNone) {
    const int differrorcat = static_cast<int>(diff_error_category());
    event.Set("differrorcat", differrorcat);
  }
  if (diff_error_code()) {
    event.Set("differrorcode", diff_error_code());
  }
  if (diff_extra_code1()) {
    event.Set("diffextracode1", diff_extra_code1());
  }
  if (!previous_fp().empty()) {
    event.Set("previousfp", previous_fp());
  }
  if (!next_fp().empty()) {
    event.Set("nextfp", next_fp());
  }
  return event;
}

std::vector<base::Value::Dict> Component::GetEvents() const {
  std::vector<base::Value::Dict> events;
  for (const auto& event : events_) {
    events.push_back(event.Clone());
  }
  return events;
}

std::unique_ptr<CrxInstaller::InstallParams> Component::install_params() const {
  return install_params_
             ? std::make_unique<CrxInstaller::InstallParams>(*install_params_)
             : nullptr;
}

Component::State::State(Component* component, ComponentState state)
    : state_(state), component_(*component) {}

Component::State::~State() = default;

void Component::State::Handle(CallbackNextState callback_next_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  callback_next_state_ = std::move(callback_next_state);

  DoHandle();
}

void Component::State::TransitionState(std::unique_ptr<State> next_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(next_state);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_next_state_), std::move(next_state)));
}

void Component::State::EndState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_next_state_), nullptr));
}

Component::StateNew::StateNew(Component* component)
    : State(component, ComponentState::kNew) {}

Component::StateNew::~StateNew() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateNew::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = State::component();
  if (component.crx_component()) {
    TransitionState(std::make_unique<StateChecking>(&component));

    // Notify that the component is being checked for updates after the
    // transition to `StateChecking` occurs. This event indicates the start
    // of the update check. The component receives the update check results when
    // the update checks completes, and after that, `UpdateEngine` invokes the
    // function `StateChecking::DoHandle` to transition the component out of
    // the `StateChecking`. The current design allows for notifying observers
    // on state transitions but it does not allow such notifications when a
    // new state is entered. Hence, posting the task below is a workaround for
    // this design oversight.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&Component::NotifyObservers,
                                  base::Unretained(&component)));
  } else {
    component.error_code_ = static_cast<int>(Error::CRX_NOT_FOUND);
    component.error_category_ = ErrorCategory::kService;
    TransitionState(std::make_unique<StateUpdateError>(&component));
  }
}

Component::StateChecking::StateChecking(Component* component)
    : State(component, ComponentState::kChecking) {
  component->last_check_ = base::TimeTicks::Now();
}

Component::StateChecking::~StateChecking() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateChecking::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = State::component();
  CHECK(component.crx_component());

  if (component.error_code_) {
    metrics::RecordUpdateCheckResult(metrics::UpdateCheckResult::kError);
    TransitionState(std::make_unique<StateUpdateError>(&component));
    return;
  }

  if (component.update_context_->is_cancelled) {
    metrics::RecordUpdateCheckResult(metrics::UpdateCheckResult::kCanceled);
    TransitionState(std::make_unique<StateUpdateError>(&component));
    component.error_category_ = ErrorCategory::kService;
    component.error_code_ = static_cast<int>(ServiceError::CANCELLED);
    return;
  }

  if (component.pipeline_.has_value()) {
    metrics::RecordUpdateCheckResult(metrics::UpdateCheckResult::kHasUpdate);
    TransitionState(std::make_unique<StateCanUpdate>(&component));
    return;
  }

  if (component.pipeline_.error().category_ == ErrorCategory::kNone) {
    metrics::RecordUpdateCheckResult(metrics::UpdateCheckResult::kNoUpdate);
    TransitionState(std::make_unique<StateUpToDate>(&component));
    return;
  }

  metrics::RecordUpdateCheckResult(metrics::UpdateCheckResult::kError);
  TransitionState(std::make_unique<StateUpdateError>(&component));
}

Component::StateUpdateError::StateUpdateError(Component* component)
    : State(component, ComponentState::kUpdateError) {}

Component::StateUpdateError::~StateUpdateError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateUpdateError::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = State::component();

  CHECK_NE(ErrorCategory::kNone, component.error_category_);
  CHECK_NE(0, component.error_code_);

  // Create an event only when the server response included an update.
  if (component.IsUpdateAvailable()) {
    component.AppendEvent(component.MakeEventUpdateComplete());
  }

  EndState();
  component.NotifyObservers();
}

Component::StateCanUpdate::StateCanUpdate(Component* component)
    : State(component, ComponentState::kCanUpdate) {}

Component::StateCanUpdate::~StateCanUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateCanUpdate::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = State::component();
  CHECK(component.crx_component());

  component.is_update_available_ = true;
  component.NotifyObservers();

  if (!component.crx_component()->updates_enabled ||
      (!component.crx_component()->allow_updates_on_metered_connection &&
       component.config()->IsConnectionMetered())) {
    component.error_category_ = ErrorCategory::kService;
    component.error_code_ = static_cast<int>(ServiceError::UPDATE_DISABLED);
    component.extra_code1_ = 0;
    metrics::RecordCanUpdateResult(metrics::CanUpdateResult::kUpdatesDisabled);
    TransitionState(std::make_unique<StateUpdateError>(&component));
    return;
  }

  if (component.update_context_->is_cancelled) {
    TransitionState(std::make_unique<StateUpdateError>(&component));
    component.error_category_ = ErrorCategory::kService;
    component.error_code_ = static_cast<int>(ServiceError::CANCELLED);
    metrics::RecordCanUpdateResult(metrics::CanUpdateResult::kCanceled);
    return;
  }

  if (component.update_context_->is_update_check_only) {
    component.error_category_ = ErrorCategory::kService;
    component.error_code_ =
        static_cast<int>(ServiceError::CHECK_FOR_UPDATE_ONLY);
    component.extra_code1_ = 0;
    component.AppendEvent(component.MakeEventUpdateComplete());
    EndState();
    metrics::RecordCanUpdateResult(
        metrics::CanUpdateResult::kCheckForUpdateOnly);
    return;
  }

  metrics::RecordCanUpdateResult(metrics::CanUpdateResult::kCanUpdate);

  // Start computing the cost of the this update from here on.
  component.update_begin_ = base::TimeTicks::Now();
  TransitionState(std::make_unique<StateUpdating>(&component));
}

Component::StateUpToDate::StateUpToDate(Component* component)
    : State(component, ComponentState::kUpToDate) {}

Component::StateUpToDate::~StateUpToDate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateUpToDate::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = State::component();
  CHECK(component.crx_component());

  component.NotifyObservers();
  EndState();
}

Component::StateUpdating::StateUpdating(Component* component)
    : State(component, ComponentState::kUpdating) {}

Component::StateUpdating::~StateUpdating() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateUpdating::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto& component = Component::State::component();
  if (component.pipeline_.has_value()) {
    cancel_callback_ =
        std::move(component.pipeline_.value())
            .Run(base::BindOnce(&Component::StateUpdating::PipelineComplete,
                                base::Unretained(this)));
    return;
  }
  component.error_category_ = component.pipeline_.error().category_;
  component.error_code_ = component.pipeline_.error().code_;
  component.extra_code1_ = component.pipeline_.error().extra_;
  TransitionState(std::make_unique<StateUpdateError>(&component));
}

void Component::StateUpdating::PipelineComplete(
    const CategorizedError& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = Component::State::component();

  if (result.category_ != ErrorCategory::kNone) {
    component.error_category_ = result.category_;
    component.error_code_ = result.code_;
    component.extra_code1_ = result.extra_;
  }

  CHECK(component.crx_component_);
  if (!component.crx_component_->allow_cached_copies) {
    component.update_context_->crx_cache_->RemoveAll(
        component.crx_component()->app_id);
  }

  if (component.error_category_ != ErrorCategory::kNone) {
    TransitionState(std::make_unique<StateUpdateError>(&component));
    return;
  }

  CHECK_EQ(ErrorCategory::kNone, component.error_category_);
  TransitionState(std::make_unique<StateUpdated>(&component));
}

Component::StateUpdated::StateUpdated(Component* component)
    : State(component, ComponentState::kUpdated) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Component::StateUpdated::~StateUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateUpdated::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = State::component();
  CHECK(component.crx_component());

  component.crx_component_->version = component.next_version_;
  component.crx_component_->fingerprint = component.next_fp_;

  component.update_context_->persisted_data->SetProductVersion(
      component.id(), component.crx_component_->version);
  component.update_context_->persisted_data->SetMaxPreviousProductVersion(
      component.id(), component.previous_version_);
  component.update_context_->persisted_data->SetFingerprint(
      component.id(), component.crx_component_->fingerprint);

  component.AppendEvent(component.MakeEventUpdateComplete());

  component.NotifyObservers();
  metrics::RecordComponentUpdated();
  EndState();
}

}  // namespace update_client
