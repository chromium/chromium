// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/component.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
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

// The state machine representing how a CRX component changes during an update.
//
//     +------------------------- kNew
//     |                            |
//     |                            V
//     |                        kChecking
//     |                            |
//     V                error       V     no           no
//  kUpdateError <------------- [update?] -> [action?] -> kUpToDate
//     ^                            |           |            ^
//     |                        yes |           | yes        |
//     |     update disabled        V           |            |
//     +-<--------------------- kCanUpdate      +--------> kRun
//     |                            |
//     |                            V           yes
//     |                    [download cached?] --------------+
//     |                               |                     |
//     |                            no |                     |
//     |                no             |                     |
//     |               +-<- [differential update?]           |
//     |               |               |                     |
//     |               |           yes |                     |
//     |               |               |                     |
//     |    error, no  |               |                     |
//     +-<----------[disk space available?]                  |
//     |               |               |                     |
//     |           yes |           yes |                     |
//     |               |               |                     |
//     |               |               |                     |
//     |               | error         V                     |
//     |               +-<----- kDownloadingDiff             |
//     |               |               |                     |
//     |               |               |                     |
//     |               | error         V                     |
//     |               +-<----- kUpdatingDiff                |
//     |               |               |                     |
//     |    error      V               |                     |
//     +-<-------- kDownloading        |                     |
//     |               |               |                     |
//     |               |               |                     |
//     |    error      V               V      no             |
//     +-<-------- kUpdating -----> [action?] -> kUpdated    |
//                     ^               |            ^        |
//                     |               | yes        |        |
//                     |               |            |        |
//                     |               +--------> kRun       |
//                     |                                     |
//                     +-------------------------------------+

// The state machine for a check for update only.
//
//                                kNew
//                                  |
//                                  V
//                             kChecking
//                                  |
//                         yes      V     no
//                         +----[update?] ------> kUpToDate
//                         |
//             yes         v           no
//          +---<-- update disabled? -->---+
//          |                              |
//     kUpdateError                    kCanUpdate

namespace update_client {

Component::Component(const UpdateContext& update_context, const std::string& id)
    : id_(id),
      state_(std::make_unique<StateNew>(this)),
      update_context_(update_context) {}

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

  previous_state_ = state();
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

  CrxUpdateItem crx_update_item;
  crx_update_item.state = state_->state();
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

void Component::SetParseResult(const ProtocolParser::Result& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK_EQ(0, update_check_error_);

  status_ = result.status;
  action_run_ = result.action_run;
  custom_attrs_ = result.custom_attributes;

  if (result.manifest.packages.empty()) {
    return;
  }

  next_version_ = base::Version(result.manifest.version);
  const auto& package = result.manifest.packages.front();
  next_fp_ = package.fingerprint;

  // Resolve the urls by combining the base urls with the package names.
  for (const auto& crx_url : result.crx_urls) {
    const GURL url = crx_url.Resolve(package.name);
    if (url.is_valid()) {
      crx_urls_.push_back(url);
    }
  }
  for (const auto& crx_diffurl : result.crx_diffurls) {
    const GURL url = crx_diffurl.Resolve(package.namediff);
    if (url.is_valid()) {
      crx_diffurls_.push_back(url);
    }
  }

  hash_sha256_ = package.hash_sha256;
  hashdiff_sha256_ = package.hashdiff_sha256;

  size_ = package.size;
  sizediff_ = package.sizediff;

  if (!result.manifest.run.empty()) {
    install_params_ = std::make_optional(CrxInstaller::InstallParams(
        result.manifest.run, result.manifest.arguments,
        [&result](const std::string& expected) -> std::string {
          if (expected.empty() || result.data.empty()) {
            return "";
          }

          const auto it = base::ranges::find(
              result.data, expected,
              &ProtocolParser::Result::Data::install_data_index);

          const bool matched = it != std::end(result.data);
          DVLOG(2) << "Expected install_data_index: " << expected
                   << ", matched: " << matched;

          return matched ? it->text : "";
        }(crx_component_ ? crx_component_->install_data_index : "")));
  }
}

void Component::SetUpdateCheckResult(
    const std::optional<ProtocolParser::Result>& result,
    ErrorCategory error_category,
    int error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(ComponentState::kChecking, state());

  error_category_ = error_category;
  error_code_ = error;

  if (result) {
    SetParseResult(result.value());
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

base::Value::Dict Component::MakeEventActionRun(bool succeeded,
                                                int error_code,
                                                int extra_code1) const {
  base::Value::Dict event;
  event.Set("eventtype", protocol_request::kEventAction);
  event.Set("eventresult", static_cast<int>(succeeded));
  if (error_code) {
    event.Set("errorcode", error_code);
  }
  if (extra_code1) {
    event.Set("extracode1", extra_code1);
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

  if (component.status_ == "ok") {
    metrics::RecordUpdateCheckResult(metrics::UpdateCheckResult::kHasUpdate);
    TransitionState(std::make_unique<StateCanUpdate>(&component));
    return;
  }

  if (component.status_ == "noupdate") {
    metrics::RecordUpdateCheckResult(metrics::UpdateCheckResult::kNoUpdate);
    if (component.action_run_.empty() ||
        component.update_context_->is_update_check_only) {
      TransitionState(std::make_unique<StateUpToDate>(&component));
    } else {
      TransitionState(std::make_unique<StateRun>(&component));
    }
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
  CHECK(component.update_context_->crx_cache_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &update_client::CrxCache::Get,
          component.update_context_->crx_cache_.value(),
          component.crx_component()->app_id, component.next_fp_,
          base::BindOnce(
              &Component::StateCanUpdate::GetNextCrxFromCacheComplete,
              base::Unretained(this))));
}

// Returns true if a differential update is available, it has not failed yet,
// and the configuration allows this update.
bool Component::StateCanUpdate::CanTryDiffUpdate() const {
  const auto& component = Component::State::component();
  return component.HasDiffUpdate() && !component.diff_error_code_ &&
         component.update_context_->crx_cache_.has_value() &&
         component.update_context_->config->EnabledDeltas();
}

void Component::StateCanUpdate::GetNextCrxFromCacheComplete(
    const CrxCache::Result& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto& component = State::component();
  if (result.error == UnpackerError::kNone) {
    component.payload_path_ = result.crx_cache_path;
    TransitionState(std::make_unique<StateUpdating>(&component));
    return;
  }
  if (CanTryDiffUpdate()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&update_client::CrxCache::Contains,
                       component.update_context_->crx_cache_.value(),
                       component.crx_component()->app_id,
                       component.previous_fp_),
        base::BindOnce(
            &Component::StateCanUpdate::CheckIfCacheContainsPreviousCrxComplete,
            base::Unretained(this)));
    return;
  }
  TransitionState(std::make_unique<StateDownloading>(&component, false));
}

void Component::StateCanUpdate::CheckIfCacheContainsPreviousCrxComplete(
    bool crx_is_in_cache) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto& component = State::component();
  if (crx_is_in_cache) {
    TransitionState(std::make_unique<StateDownloading>(&component, true));
  } else {
    // If the configuration allows diff update, but the previous crx
    // is not cached, report the kPuffinMissingPreviousCrx error.
    component.diff_error_category_ = ErrorCategory::kUnpack;
    component.diff_error_code_ =
        static_cast<int>(UnpackerError::kPuffinMissingPreviousCrx);
    TransitionState(std::make_unique<StateDownloading>(&component, false));
  }
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

Component::StateDownloading::StateDownloading(Component* component, bool diff)
    : State(component,
            diff ? ComponentState::kDownloadingDiff
                 : ComponentState::kDownloading),
      diff_(diff) {}

Component::StateDownloading::~StateDownloading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateDownloading::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Component& component = Component::State::component();

  component.downloaded_bytes_ = -1;
  component.total_bytes_ = -1;

  cancel_callback_ = DownloadOperation(
      base::WrapRefCounted(&*(component.update_context_)),
      diff_ ? component.crx_diffurls_ : component.crx_urls_,
      diff_ ? component.sizediff_ : component.size_,
      diff_ ? component.hashdiff_sha256_ : component.hash_sha256_,
      base::BindRepeating(&Component::AppendEvent,
                          base::Unretained(&component)),
      base::BindRepeating(
          [](base::raw_ref<Component> component, int64_t downloaded_bytes,
             int64_t total_bytes) {
            component->downloaded_bytes_ = downloaded_bytes;
            component->total_bytes_ = total_bytes;
            component->NotifyObservers();
          },
          base::raw_ref(component)),
      base::BindOnce(&Component::StateDownloading::DownloadComplete,
                     base::Unretained(this)));
  component.NotifyObservers();
}

void Component::StateDownloading::DownloadComplete(
    const base::expected<base::FilePath, CategorizedError>& file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Component& component = Component::State::component();

  if (!file.has_value()) {
    if (diff_) {
      component.diff_error_category_ = file.error().category_;
      component.diff_error_code_ = file.error().code_;
      component.diff_extra_code1_ = file.error().extra_;
      TransitionState(
          std::make_unique<Component::StateDownloading>(&component, false));
    } else {
      component.error_category_ = file.error().category_;
      component.error_code_ = file.error().code_;
      component.extra_code1_ = file.error().extra_;
      TransitionState(
          std::make_unique<Component::StateUpdateError>(&component));
    }
    return;
  }

  component.payload_path_ = *file;
  if (diff_) {
    TransitionState(std::make_unique<Component::StateUpdatingDiff>(&component));
  } else {
    TransitionState(std::make_unique<Component::StateUpdating>(&component));
  }
}

Component::StateUpdatingDiff::StateUpdatingDiff(Component* component)
    : State(component, ComponentState::kUpdatingDiff) {}

Component::StateUpdatingDiff::~StateUpdatingDiff() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateUpdatingDiff::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = Component::State::component();
  CHECK(component.crx_component());

  component.install_progress_ = -1;
  component.NotifyObservers();

  PuffOperation(
      component.update_context_->crx_cache_,
      component.update_context_->config->GetPatcherFactory()->Create(),
      base::BindRepeating(&Component::AppendEvent,
                          base::Unretained(&component)),
      component.crx_component()->app_id, component.previous_fp_,
      component.payload_path_, component.payload_path_.DirName(),
      base::BindOnce(&Component::StateUpdatingDiff::PatchingComplete,
                     base::Unretained(this)));
}

void Component::StateUpdatingDiff::PatchingComplete(
    const base::expected<base::FilePath, CategorizedError>& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto& component = Component::State::component();
  CHECK(component.crx_component());

  if (!result.has_value()) {
    InstallComplete(CrxInstaller::Result(result.error()));
    return;
  }

  InstallOperation(
      component.update_context_->crx_cache_,
      component.update_context_->config->GetUnzipperFactory()->Create(),
      component.crx_component()->crx_format_requirement,
      component.crx_component()->app_id, component.crx_component()->pk_hash,
      component.crx_component()->installer, component.install_params(),
      component.next_fp_,
      base::BindRepeating(&Component::AppendEvent,
                          base::Unretained(&component)),
      base::BindOnce(&Component::StateUpdatingDiff::InstallComplete,
                     base::Unretained(this)),
      base::BindRepeating(&Component::StateUpdatingDiff::InstallProgress,
                          base::Unretained(this)),
      result.value());
}

void Component::StateUpdatingDiff::InstallProgress(int install_progress) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = Component::State::component();
  if (install_progress >= 0 && install_progress <= 100) {
    component.install_progress_ = install_progress;
  }
  component.NotifyObservers();
}

void Component::StateUpdatingDiff::InstallComplete(
    const CrxInstaller::Result& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = Component::State::component();

  component.diff_error_category_ = result.result.category_;
  component.diff_error_code_ = result.result.code_;
  component.diff_extra_code1_ = result.result.extra_;
  component.installer_result_ = result;

  if (component.diff_error_category_ != ErrorCategory::kNone) {
    TransitionState(std::make_unique<StateDownloading>(&component, false));
    return;
  }

  CHECK_EQ(ErrorCategory::kNone, component.diff_error_category_);
  CHECK_EQ(ErrorCategory::kNone, component.error_category_);

  if (component.action_run_.empty()) {
    TransitionState(std::make_unique<StateUpdated>(&component));
  } else {
    TransitionState(std::make_unique<StateRun>(&component));
  }
}

Component::StateUpdating::StateUpdating(Component* component)
    : State(component, ComponentState::kUpdating) {}

Component::StateUpdating::~StateUpdating() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateUpdating::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = Component::State::component();
  const auto& update_context = *component.update_context_;

  CHECK(component.crx_component());

  component.install_progress_ = -1;
  component.NotifyObservers();

  InstallOperation(
      component.crx_component()->allow_cached_copies &&
              update_context.config->EnabledDeltas()
          ? update_context.crx_cache_
          : std::nullopt,
      component.update_context_->config->GetUnzipperFactory()->Create(),
      component.crx_component()->crx_format_requirement,
      component.crx_component()->app_id, component.crx_component()->pk_hash,
      component.crx_component()->installer, component.install_params(),
      component.next_fp_,
      base::BindRepeating(&Component::AppendEvent,
                          base::Unretained(&component)),
      base::BindOnce(&Component::StateUpdating::InstallComplete,
                     base::Unretained(this)),
      base::BindRepeating(&Component::StateUpdating::InstallProgress,
                          base::Unretained(this)),
      component.payload_path_);
}

void Component::StateUpdating::InstallProgress(int install_progress) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = Component::State::component();
  if (install_progress >= 0 && install_progress <= 100) {
    component.install_progress_ = install_progress;
  }
  component.NotifyObservers();
}

void Component::StateUpdating::InstallComplete(
    const CrxInstaller::Result& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = Component::State::component();

  component.error_category_ = result.result.category_;
  component.error_code_ = result.result.code_;
  component.extra_code1_ = result.result.extra_;
  component.installer_result_ = result;

  CHECK(component.crx_component_);
  if (!component.crx_component_->allow_cached_copies &&
      component.update_context_->crx_cache_) {
    base::ThreadPool::CreateSequencedTaskRunner(kTaskTraits)
        ->PostTask(FROM_HERE,
                   base::BindOnce(&CrxCache::RemoveAll,
                                  component.update_context_->crx_cache_->get(),
                                  component.crx_component()->app_id));
  }

  if (component.error_category_ != ErrorCategory::kNone) {
    TransitionState(std::make_unique<StateUpdateError>(&component));
    return;
  }

  CHECK_EQ(ErrorCategory::kNone, component.error_category_);

  if (component.action_run_.empty()) {
    TransitionState(std::make_unique<StateUpdated>(&component));
  } else {
    TransitionState(std::make_unique<StateRun>(&component));
  }
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

Component::StateRun::StateRun(Component* component)
    : State(component, ComponentState::kRun) {}

Component::StateRun::~StateRun() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateRun::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& component = State::component();
  CHECK(component.crx_component());

  RunAction(
      component.crx_component()->action_handler,
      component.crx_component()->installer, component.action_run(),
      component.session_id(),
      base::BindOnce(&StateRun::ActionRunComplete, base::Unretained(this)));
}

void Component::StateRun::ActionRunComplete(bool succeeded,
                                            int error_code,
                                            int extra_code1) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = State::component();

  component.AppendEvent(
      component.MakeEventActionRun(succeeded, error_code, extra_code1));
  switch (component.previous_state_) {
    case ComponentState::kChecking:
      TransitionState(std::make_unique<StateUpToDate>(&component));
      return;
    case ComponentState::kUpdating:
    case ComponentState::kUpdatingDiff:
      TransitionState(std::make_unique<StateUpdated>(&component));
      return;
    default:
      break;
  }
  NOTREACHED_IN_MIGRATION();
}

}  // namespace update_client
