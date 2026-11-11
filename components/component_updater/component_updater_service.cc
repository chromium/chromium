// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/component_updater_service.h"

#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service_internal.h"
#include "components/component_updater/component_updater_utils.h"
#include "components/component_updater/pref_names.h"
#include "components/component_updater/required_components_controller.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/configurator.h"
#include "components/update_client/crx_cache.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "net/base/network_change_notifier.h"
#include "url/gurl.h"

namespace {

using CrxInstaller = ::update_client::CrxInstaller;
using UpdateClient = ::update_client::UpdateClient;

enum UpdateType {
  UPDATE_TYPE_MANUAL = 0,
  UPDATE_TYPE_AUTOMATIC,
  UPDATE_TYPE_COUNT,
};

// Delay before resuming deferred updates once the connection is no longer
// metered, so that a connection that turns metered again right away does not
// restart the downloads.
constexpr base::TimeDelta kResumeDeferredUpdatesDelay = base::Seconds(5);

// Returns true if the update client refused the update in `item` only
// because the connection is metered.
bool IsRefusedOnMeteredConnection(
    const component_updater::CrxUpdateItem& item) {
  return item.state == update_client::ComponentState::kUpdateError &&
         item.error_category == update_client::ErrorCategory::kService &&
         item.error_code ==
             std::to_underlying(update_client::ServiceError::UPDATE_DISABLED) &&
         item.component && item.component->updates_enabled &&
         !item.component->allow_updates_on_metered_connection;
}

}  // namespace

namespace component_updater {

CrxUpdateService::CrxUpdateService(scoped_refptr<Configurator> config,
                                   std::unique_ptr<UpdateScheduler> scheduler,
                                   scoped_refptr<UpdateClient> update_client,
                                   const std::string& brand,
                                   bool update_checks_disabled)
    : config_(config),
      scheduler_(std::move(scheduler)),
      update_client_(update_client),
      brand_(brand),
      update_checks_disabled_(update_checks_disabled) {
  update_client->CleanupStaleDownloads(
      base::Time::Now(),
      base::BindOnce([] { VLOG(2) << "CleanupStaleDownloads done"; }));

#if BUILDFLAG(CHROME_FOR_TESTING)
  if (auto components = config_->GetRequiredComponents(); !components.empty()) {
    required_components_controller_ =
        std::make_unique<RequiredComponentsController>(std::move(components));
  }
#endif

  AddObserver(this);
  net::NetworkChangeNotifier::AddConnectionCostObserver(this);
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

CrxUpdateService::~CrxUpdateService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& item : ready_callbacks_) {
    std::move(item.second).Run();
  }

  RemoveObserver(this);

  Stop();
}

void CrxUpdateService::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  update_client_->AddObserver(observer);
}

void CrxUpdateService::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  update_client_->RemoveObserver(observer);
}

base::Version CrxUpdateService::GetRegisteredVersion(
    const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Version registered_version =
      config_->GetPersistedData()->GetProductVersion(app_id);
  return registered_version.IsValid() ? registered_version
                                      : base::Version(kNullVersion);
}

base::Version CrxUpdateService::GetMaxPreviousProductVersion(
    const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Version max_previous_product_version =
      config_->GetPersistedData()->GetMaxPreviousProductVersion(app_id);
  return max_previous_product_version.IsValid() ? max_previous_product_version
                                                : base::Version(kNullVersion);
}

void CrxUpdateService::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (update_checks_disabled_) {
    VLOG(1) << "CrxUpdateService update checks are disabled";
    return;
  }
  VLOG(1) << "CrxUpdateService starting up. "
          << "First update attempt will take place in "
          << config_->InitialDelay() << " seconds. "
          << "Next update attempt will take place in "
          << config_->NextCheckDelay() << " seconds. ";

  scheduler_->Schedule(
      config_->InitialDelay(), config_->NextCheckDelay(),
      base::BindRepeating(
          base::IgnoreResult(&CrxUpdateService::CheckForUpdates),
          weak_ptr_factory_.GetWeakPtr()),
      base::DoNothing());
}

// Stops the update loop. In flight operations will be completed.
void CrxUpdateService::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "CrxUpdateService stopping";
  is_stopped_ = true;
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  net::NetworkChangeNotifier::RemoveConnectionCostObserver(this);
  resume_deferred_updates_timer_.Stop();
  scheduler_->Stop();
  update_client_->Stop();
}

// Adds a component to be checked for upgrades. If the component exists it
// it will be replaced.
bool CrxUpdateService::RegisterComponent(
    const ComponentRegistration& component) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (component.app_id.empty() || !component.version.IsValid() ||
      !component.installer) {
    return false;
  }

  // Cancel any pending unregistration for this component.
  std::erase(components_pending_unregistration_, component.app_id);

  // Update the registration data if the component has been registered before.
  auto it = components_.find(component.app_id);
  if (it != components_.end()) {
    it->second = component;
    return true;
  }

  components_.insert(std::make_pair(component.app_id, component));
  components_order_.push_back(component.app_id);

  // Create an initial state for this component. The state is mutated in
  // response to events from the UpdateClient instance.
  const auto [unused, inserted] = component_states_.insert(std::make_pair(
      component.app_id, CrxUpdateItem{.id = component.app_id,
                                      .component = ToCrxComponent(component)}));
  CHECK(inserted);

#if BUILDFLAG(CHROME_FOR_TESTING)
  if (required_components_controller_ &&
      required_components_controller_->RequestComponentUpdate(component)) {
    OnDemandUpdateInternal(component.app_id,
                           OnDemandUpdater::Priority::FOREGROUND,
                           base::DoNothing());
  }
#endif

  // Start the timer if this is the first component registered. The first timer
  // event occurs after an interval defined by the component update
  // configurator. The subsequent timer events are repeated with a period
  // defined by the same configurator.
  if (components_.size() == 1) {
    Start();
  }

  return true;
}

bool CrxUpdateService::UnregisterComponent(const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = components_.find(id);
  if (it == components_.end()) {
    return false;
  }

  if (deferred_updates_.erase(id) && deferred_updates_.empty()) {
    resume_deferred_updates_timer_.Stop();
  }

  // Delay the uninstall of the component if the component is being updated.
  if (update_client_->IsUpdating(id)) {
    components_pending_unregistration_.push_back(id);
    return true;
  }

  return DoUnregisterComponent(id);
}

bool CrxUpdateService::DoUnregisterComponent(const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const bool result = components_.find(id)->second.installer->Uninstall();

  const auto pos = std::ranges::find(components_order_, id);
  if (pos != components_order_.end()) {
    components_order_.erase(pos);
  }

  components_.erase(id);
  component_states_.erase(id);
  if (deferred_updates_.erase(id) && deferred_updates_.empty()) {
    resume_deferred_updates_timer_.Stop();
  }

  return result;
}

std::vector<std::string> CrxUpdateService::GetComponentIDs() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::string> ids;
  for (const auto& [app_id, registration] : components_) {
    ids.push_back(app_id);
  }
  return ids;
}

std::vector<ComponentInfo> CrxUpdateService::GetComponents() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<ComponentInfo> result;
  for (const auto& [app_id, registration] : components_) {
    result.push_back({.id = app_id,
                      .fingerprint = registration.fingerprint,
                      .name = base::UTF8ToUTF16(registration.name),
                      .version = registration.version,
                      .cohort_id = config_->GetPersistedData()->GetCohort(
                          registration.app_id)});
  }
  return result;
}

OnDemandUpdater& CrxUpdateService::GetOnDemandUpdater() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return *this;
}

#if BUILDFLAG(CHROME_FOR_TESTING)
void CrxUpdateService::EnsureRequiredComponentsReady(base::TimeDelta timeout) {
  if (required_components_controller_) {
    required_components_controller_->EnsureRequiredComponentsReady(timeout);
  }
}
#endif

update_client::CrxComponent CrxUpdateService::ToCrxComponent(
    const ComponentRegistration& component) const {
  bool component_updates_enabled =
      config_->GetPrefService()->GetBoolean(prefs::kComponentUpdatesEnabled);
  // Some components should update even when enterprise policy disables
  // updates.
  bool override_component_updates_enabled =
      !component.supports_group_policy_enable_component_updates;
  bool should_update =
      override_component_updates_enabled || component_updates_enabled;

  update_client::CrxComponent crx = {
      .pk_hash = component.public_key_hash,
      .installer = component.installer,
      .action_handler = component.action_handler,
      .app_id = component.app_id,
      .version = component.version,
      .brand = brand_,
      .fingerprint = component.fingerprint,
      .name = component.name,
      .installer_attributes = component.installer_attributes,
      .requires_network_encryption = component.requires_network_encryption,
      .crx_format_requirement =
          crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF,
      .updates_enabled = component.allow_updates && should_update,
      .allow_cached_copies = component.allow_cached_copies,
      .allow_updates_on_metered_connection =
          component.allow_updates_on_metered_connection};

#if BUILDFLAG(CHROME_FOR_TESTING)
  if (required_components_controller_) {
    required_components_controller_->ToCrxComponent(component, crx);
  }
#endif
  return crx;
}

std::optional<ComponentRegistration> CrxUpdateService::GetComponent(
    const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return component_updater::GetComponent(components_, id);
}

const CrxUpdateItem* CrxUpdateService::GetComponentState(
    const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto it(component_states_.find(id));
  return it != component_states_.end() ? &it->second : nullptr;
}

void CrxUpdateService::MaybeThrottle(const std::string& id,
                                     base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto it = components_.find(id);
  if (it != components_.end()) {
    if (OnDemandUpdateWithCooldown(id)) {
      ready_callbacks_.insert(std::make_pair(id, std::move(callback)));
      return;
    }
  }

  // Unblock the request if the request can't be throttled.
  std::move(callback).Run();
}

void CrxUpdateService::OnDemandUpdate(const std::string& id,
                                      Priority priority,
                                      Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Checked before the registration, so that callers get the same answer for
  // components that are not registered when update checks are disabled.
  if (update_checks_disabled_) {
    if (!callback.is_null()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    update_client::Error::UPDATE_DISABLED));
    }
    return;
  }

  if (!GetComponent(id)) {
    if (!callback.is_null()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    update_client::Error::INVALID_ARGUMENT));
    }
    return;
  }

  OnDemandUpdateInternal(id, priority, std::move(callback));
}

bool CrxUpdateService::OnDemandUpdateWithCooldown(const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (update_checks_disabled_) {
    return false;
  }

  // Check if the request is too soon.
  const auto* component_state(GetComponentState(id));
  if (component_state && !component_state->last_check.is_null()) {
    base::TimeDelta delta =
        base::TimeTicks::Now() - component_state->last_check;
    if (delta < config_->OnDemandDelay()) {
      return false;
    }
  }

  OnDemandUpdateInternal(id, Priority::FOREGROUND, Callback());
  return true;
}

void CrxUpdateService::OnDemandUpdateInternal(const std::string& id,
                                              Priority priority,
                                              Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!config_->IsConnectionMetered() &&
      !net::NetworkChangeNotifier::IsOffline()) {
    if (const auto it = deferred_updates_.find(id);
        it != deferred_updates_.end() &&
        (priority == Priority::FOREGROUND || !it->second)) {
      deferred_updates_.erase(it);
      if (deferred_updates_.empty()) {
        resume_deferred_updates_timer_.Stop();
      }
    }
  }

  UMA_HISTOGRAM_ENUMERATION("ComponentUpdater.Calls", UPDATE_TYPE_MANUAL,
                            UPDATE_TYPE_COUNT);
  StartOnDemandUpdate(id, priority, std::move(callback));
}

void CrxUpdateService::StartOnDemandUpdate(const std::string& id,
                                           Priority priority,
                                           Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool is_foreground = priority == Priority::FOREGROUND;
  if (is_foreground) {
    ++foreground_update_counts_[id];
  }
  update_client_->Update(
      {id},
      base::BindOnce(&CrxUpdateService::GetCrxComponents,
                     weak_ptr_factory_.GetWeakPtr()),
      /*crx_state_change_callback=*/{}, is_foreground,
      base::BindOnce(&CrxUpdateService::OnDemandUpdateComplete,
                     weak_ptr_factory_.GetWeakPtr(), id, priority,
                     std::move(callback), base::TimeTicks::Now()));
}

bool CrxUpdateService::Cancel(const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return update_client_->Cancel(id);
}

bool CrxUpdateService::CheckForUpdates(
    UpdateScheduler::OnFinishedCallback on_finished) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UMA_HISTOGRAM_ENUMERATION("ComponentUpdater.Calls", UPDATE_TYPE_AUTOMATIC,
                            UPDATE_TYPE_COUNT);

  if (components_order_.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_finished));
    return true;
  }

  config_->GetCrxCache()->RemoveIfNot(
      components_order_,
      base::BindOnce(
          &UpdateClient::Update, update_client_, components_order_,
          base::BindOnce(&CrxUpdateService::GetCrxComponents,
                         weak_ptr_factory_.GetWeakPtr()),
          base::DoNothing(), false,
          base::BindOnce(&CrxUpdateService::OnUpdateComplete,
                         weak_ptr_factory_.GetWeakPtr(),
                         base::BindOnce(
                             [](UpdateScheduler::OnFinishedCallback on_finished,
                                update_client::Error /*error*/) {
                               std::move(on_finished).Run();
                             },
                             std::move(on_finished)),
                         base::TimeTicks::Now())));
  return true;
}

bool CrxUpdateService::GetComponentDetails(const std::string& id,
                                           CrxUpdateItem* item) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // First, if this component is currently being updated, return its state from
  // the update client.
  if (update_client_->GetCrxUpdateState(id, item)) {
    return true;
  }

  // Otherwise, return the last seen state of the component, if such a
  // state exists.
  const auto component_states_it = component_states_.find(id);
  if (component_states_it != component_states_.end()) {
    *item = component_states_it->second;
    return true;
  }

  return false;
}

void CrxUpdateService::GetCrxComponents(
    const std::vector<std::string>& ids,
    base::OnceCallback<void(const std::vector<std::optional<CrxComponent>>&)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::optional<CrxComponent>> crxs;
  for (std::optional<ComponentRegistration> item :
       component_updater::GetCrxComponents(components_, ids)) {
    crxs.push_back(item ? std::optional<CrxComponent>{ToCrxComponent(*item)}
                        : std::nullopt);
  }
  std::move(callback).Run(crxs);
}

void CrxUpdateService::OnUpdateComplete(Callback callback,
                                        const base::TimeTicks& start_time,
                                        update_client::Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "Update completed with error " << std::to_underlying(error);

  UMA_HISTOGRAM_BOOLEAN("ComponentUpdater.UpdateCompleteResult",
                        error != update_client::Error::NONE);
  UMA_HISTOGRAM_ENUMERATION("ComponentUpdater.UpdateCompleteError", error);
  UMA_HISTOGRAM_LONG_TIMES_100("ComponentUpdater.UpdateCompleteTime",
                               base::TimeTicks::Now() - start_time);

  for (const auto& id : components_pending_unregistration_) {
    if (!update_client_->IsUpdating(id)) {
      const auto component = GetComponent(id);
      if (component) {
        DoUnregisterComponent(id);
      }
    }
  }

  if (!callback.is_null()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), error));
  }
}

void CrxUpdateService::OnDemandUpdateComplete(const std::string& id,
                                              Priority priority,
                                              Callback callback,
                                              base::TimeTicks start_time,
                                              update_client::Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (priority == Priority::FOREGROUND) {
    const auto it = foreground_update_counts_.find(id);
    CHECK(it != foreground_update_counts_.end());
    if (--it->second == 0) {
      foreground_update_counts_.erase(it);
    }
  }
  OnUpdateComplete(std::move(callback), start_time, error);
}

void CrxUpdateService::OnEvent(const CrxUpdateItem& update_item) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Unblock all throttles for the component.
  if (update_item.state == update_client::ComponentState::kUpdated ||
      update_item.state == update_client::ComponentState::kUpToDate ||
      update_item.state == update_client::ComponentState::kUpdateError) {
    auto [first, last] = ready_callbacks_.equal_range(update_item.id);
    for (auto it = first; it != last; ++it) {
      std::move(it->second).Run();
    }
    ready_callbacks_.erase(update_item.id);
  }

  // Update the state of the item.
  const auto state_it = component_states_.find(update_item.id);
  if (state_it != component_states_.end()) {
    state_it->second = update_item;
  }

  // Update the component registration with the new version.
  if (update_item.state == update_client::ComponentState::kUpdated) {
    const auto component_it = components_.find(update_item.id);
    if (component_it != components_.end()) {
      component_it->second.version = update_item.next_version;
      component_it->second.fingerprint = update_item.next_fp;
    }
  }

  if (update_item.state == update_client::ComponentState::kUpdated ||
      update_item.state == update_client::ComponentState::kUpToDate) {
    if (deferred_updates_.erase(update_item.id) && deferred_updates_.empty()) {
      resume_deferred_updates_timer_.Stop();
    }
  }

  // An update refused because the connection is metered is attempted again
  // once the connection is no longer metered, instead of waiting for the next
  // scheduled update check.
  if (IsRefusedOnMeteredConnection(update_item) &&
      components_.contains(update_item.id)) {
    VLOG(1) << "Deferring the update of " << update_item.id
            << " until the connection is no longer metered.";
    DeferUpdate(update_item.id, GetOnDemandPriority(update_item.id));
  }

#if BUILDFLAG(CHROME_FOR_TESTING)
  if (required_components_controller_) {
    required_components_controller_->OnEvent(update_item);
  }
#endif
}

void CrxUpdateService::OnConnectionCostChanged(
    net::NetworkChangeNotifier::ConnectionCost /*cost*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_stopped_) {
    return;
  }
  if (config_->IsConnectionMetered()) {
    resume_deferred_updates_timer_.Stop();
    CancelMeteredDownloads();
    return;
  }
  if (net::NetworkChangeNotifier::IsOffline()) {
    resume_deferred_updates_timer_.Stop();
    return;
  }
  if (!deferred_updates_.empty() &&
      !resume_deferred_updates_timer_.IsRunning()) {
    resume_deferred_updates_timer_.Start(
        FROM_HERE, kResumeDeferredUpdatesDelay, this,
        &CrxUpdateService::ResumeDeferredUpdates);
  }
}

void CrxUpdateService::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType /*type*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnConnectionCostChanged(net::NetworkChangeNotifier::CONNECTION_COST_UNKNOWN);
}

void CrxUpdateService::CancelMeteredDownloads() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& [id, component] : components_) {
    if (component.allow_updates_on_metered_connection) {
      continue;
    }
    CrxUpdateItem item;
    if (update_client_->GetCrxUpdateState(id, &item) &&
        item.state == update_client::ComponentState::kDownloading &&
        Cancel(id)) {
      VLOG(1) << "Cancelled the download of " << id
              << ": the connection is metered.";
      DeferUpdate(id, GetOnDemandPriority(id));
    }
  }
}

OnDemandUpdater::Priority CrxUpdateService::GetOnDemandPriority(
    const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return foreground_update_counts_.contains(id) ? Priority::FOREGROUND
                                                : Priority::BACKGROUND;
}

void CrxUpdateService::DeferUpdate(const std::string& id, Priority priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_stopped_ ||
      std::ranges::contains(components_pending_unregistration_, id)) {
    return;
  }
  bool& is_foreground = deferred_updates_[id];
  is_foreground = is_foreground || priority == Priority::FOREGROUND;
  if (!config_->IsConnectionMetered() &&
      !net::NetworkChangeNotifier::IsOffline() &&
      !resume_deferred_updates_timer_.IsRunning()) {
    resume_deferred_updates_timer_.Start(
        FROM_HERE, kResumeDeferredUpdatesDelay, this,
        &CrxUpdateService::ResumeDeferredUpdates);
  }
}

void CrxUpdateService::ResumeDeferredUpdates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_stopped_ || config_->IsConnectionMetered() ||
      net::NetworkChangeNotifier::IsOffline()) {
    // The connection became metered or went offline; the updates stay
    // deferred.
    return;
  }
  // Cancelling a single component completes its original UpdateClient task with
  // `Error::NONE`, so the original `OnDemandUpdate()` callback has already run
  // and retries below use an empty `Callback()`.
  const base::flat_map<std::string, bool> deferred_updates =
      std::exchange(deferred_updates_, {});
  std::vector<std::string> background_ids;
  for (const auto& [id, is_foreground] : deferred_updates) {
    VLOG(1) << "Resuming the update of " << id
            << ": the connection is no longer metered.";
    if (is_foreground) {
      StartOnDemandUpdate(id, Priority::FOREGROUND, Callback());
    } else {
      background_ids.push_back(id);
    }
  }
  if (!background_ids.empty()) {
    update_client_->Update(background_ids,
                           base::BindOnce(&CrxUpdateService::GetCrxComponents,
                                          weak_ptr_factory_.GetWeakPtr()),
                           /*crx_state_change_callback=*/{},
                           /*is_foreground=*/false,
                           base::BindOnce(&CrxUpdateService::OnUpdateComplete,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          Callback(), base::TimeTicks::Now()));
  }
}

///////////////////////////////////////////////////////////////////////////////

// The component update factory. Using the component updater as a singleton
// is the job of the browser process.
std::unique_ptr<ComponentUpdateService> ComponentUpdateServiceFactory(
    scoped_refptr<Configurator> config,
    std::unique_ptr<UpdateScheduler> scheduler,
    const std::string& brand,
    bool update_checks_disabled) {
  CHECK(config);
  CHECK(scheduler);
  auto update_client = update_client::UpdateClientFactory(config);
  return std::make_unique<CrxUpdateService>(config, std::move(scheduler),
                                            std::move(update_client), brand,
                                            update_checks_disabled);
}

// Register prefs required by the component update service.
void RegisterComponentUpdateServicePrefs(PrefRegistrySimple* registry) {
  // If the preference is not set the component updates are enabled by default
  // unless in Chrome for Testing where we never want components to be updated
  // automatically.
  static constexpr bool kComponentUpdatesEnabledByDefault =
      !BUILDFLAG(CHROME_FOR_TESTING);

  registry->RegisterBooleanPref(prefs::kComponentUpdatesEnabled,
                                kComponentUpdatesEnabledByDefault);
}

}  // namespace component_updater
