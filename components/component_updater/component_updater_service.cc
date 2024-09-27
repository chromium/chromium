// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/component_updater_service.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/branding_buildflags.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service_internal.h"
#include "components/component_updater/component_updater_utils.h"
#include "components/component_updater/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/configurator.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "url/gurl.h"

using CrxInstaller = update_client::CrxInstaller;
using UpdateClient = update_client::UpdateClient;

namespace {

enum UpdateType {
  UPDATE_TYPE_MANUAL = 0,
  UPDATE_TYPE_AUTOMATIC,
  UPDATE_TYPE_COUNT,
};

}  // namespace

namespace component_updater {

ComponentInfo::ComponentInfo(const std::string& id,
                             const std::string& fingerprint,
                             const std::u16string& name,
                             const base::Version& version,
                             const std::string& cohort_id)
    : id(id),
      fingerprint(fingerprint),
      name(name),
      version(version),
      cohort_id(cohort_id) {}
ComponentInfo::ComponentInfo(const ComponentInfo& other) = default;
ComponentInfo& ComponentInfo::operator=(const ComponentInfo& other) = default;
ComponentInfo::ComponentInfo(ComponentInfo&& other) = default;
ComponentInfo& ComponentInfo::operator=(ComponentInfo&& other) = default;
ComponentInfo::~ComponentInfo() = default;

ComponentRegistration::ComponentRegistration(
    const std::string& app_id,
    const std::string& name,
    std::vector<uint8_t> public_key_hash,
    const base::Version& version,
    const std::string& fingerprint,
    std::map<std::string, std::string> installer_attributes,
    scoped_refptr<update_client::ActionHandler> action_handler,
    scoped_refptr<update_client::CrxInstaller> installer,
    bool requires_network_encryption,
    bool supports_group_policy_enable_component_updates,
    bool allow_cached_copies,
    bool allow_updates_on_metered_connection,
    bool allow_updates)
    : app_id(app_id),
      name(name),
      public_key_hash(public_key_hash),
      version(version),
      fingerprint(fingerprint),
      installer_attributes(installer_attributes),
      action_handler(action_handler),
      installer(installer),
      requires_network_encryption(requires_network_encryption),
      supports_group_policy_enable_component_updates(
          supports_group_policy_enable_component_updates),
      allow_cached_copies(allow_cached_copies),
      allow_updates_on_metered_connection(allow_updates_on_metered_connection),
      allow_updates(allow_updates) {}
ComponentRegistration::ComponentRegistration(
    const ComponentRegistration& other) = default;
ComponentRegistration& ComponentRegistration::operator=(
    const ComponentRegistration& other) = default;
ComponentRegistration::ComponentRegistration(ComponentRegistration&& other) =
    default;
ComponentRegistration& ComponentRegistration::operator=(
    ComponentRegistration&& other) = default;
ComponentRegistration::~ComponentRegistration() = default;

CrxUpdateService::CrxUpdateService(scoped_refptr<Configurator> config,
                                   std::unique_ptr<UpdateScheduler> scheduler,
                                   scoped_refptr<UpdateClient> update_client,
                                   const std::string& brand)
    : config_(config),
      scheduler_(std::move(scheduler)),
      update_client_(update_client),
      brand_(brand) {
  AddObserver(this);
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
  VLOG(1) << "CrxUpdateService starting up. "
          << "First update attempt will take place in "
          << config_->InitialDelay() << " seconds. "
          << "Next update attempt will take place in "
          << config_->NextCheckDelay() << " seconds. ";

  scheduler_->Schedule(
      config_->InitialDelay(), config_->NextCheckDelay(),
      base::BindRepeating(
          base::IgnoreResult(&CrxUpdateService::CheckForUpdates),
          base::Unretained(this)),
      base::DoNothing());
}

// Stops the update loop. In flight operations will be completed.
void CrxUpdateService::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "CrxUpdateService stopping";
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
  CrxUpdateItem item;
  item.id = component.app_id;
  item.component = ToCrxComponent(component);
  const auto [unused, inserted] =
      component_states_.insert(std::make_pair(component.app_id, item));
  CHECK(inserted);

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

  const auto pos = base::ranges::find(components_order_, id);
  if (pos != components_order_.end()) {
    components_order_.erase(pos);
  }

  components_.erase(id);
  component_states_.erase(id);

  return result;
}

std::vector<std::string> CrxUpdateService::GetComponentIDs() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::string> ids;
  for (const auto& it : components_) {
    ids.push_back(it.first);
  }
  return ids;
}

std::vector<ComponentInfo> CrxUpdateService::GetComponents() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<ComponentInfo> result;
  for (const auto& [app_id, registration] : components_) {
    result.emplace_back(
        app_id, registration.fingerprint, base::UTF8ToUTF16(registration.name),
        registration.version,
        config_->GetPersistedData()->GetCohort(registration.app_id));
  }
  return result;
}

OnDemandUpdater& CrxUpdateService::GetOnDemandUpdater() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return *this;
}

update_client::CrxComponent CrxUpdateService::ToCrxComponent(
    const ComponentRegistration& component) const {
  update_client::CrxComponent crx;
  crx.pk_hash = component.public_key_hash;
  crx.app_id = component.app_id;
  crx.installer = component.installer;
  crx.action_handler = component.action_handler;
  crx.version = component.version;
  crx.fingerprint = component.fingerprint;
  crx.name = component.name;
  crx.installer_attributes = component.installer_attributes;
  crx.requires_network_encryption = component.requires_network_encryption;
  crx.allow_cached_copies = component.allow_cached_copies;
  crx.allow_updates_on_metered_connection =
      component.allow_updates_on_metered_connection;

  crx.brand = brand_;
  crx.crx_format_requirement =
      crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF;

  bool component_updates_enabled =
      config_->GetPrefService()->GetBoolean(prefs::kComponentUpdatesEnabled);
  // Some components should update even when enterprise policy disables
  // updates.
  bool override_component_updates_enabled =
    !component.supports_group_policy_enable_component_updates;
  bool should_update =
      override_component_updates_enabled || component_updates_enabled;
  crx.updates_enabled = component.allow_updates && should_update;

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

  UMA_HISTOGRAM_ENUMERATION("ComponentUpdater.Calls", UPDATE_TYPE_MANUAL,
                            UPDATE_TYPE_COUNT);

  auto crx_data_callback = base::BindOnce(&CrxUpdateService::GetCrxComponents,
                                          base::Unretained(this));
  auto update_complete_callback = base::BindOnce(
      &CrxUpdateService::OnUpdateComplete, base::Unretained(this),
      std::move(callback), base::TimeTicks::Now());

  if (priority == Priority::FOREGROUND) {
    update_client_->Install(id, std::move(crx_data_callback), {},
                            std::move(update_complete_callback));
  } else if (priority == Priority::BACKGROUND) {
    update_client_->Update({id}, std::move(crx_data_callback), {}, false,
                           std::move(update_complete_callback));
  } else {
    NOTREACHED_IN_MIGRATION();
  }
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

  update_client_->Update(
      components_order_,
      base::BindOnce(&CrxUpdateService::GetCrxComponents,
                     base::Unretained(this)),
      {}, false,
      base::BindOnce(&CrxUpdateService::OnUpdateComplete,
                     base::Unretained(this),
                     base::BindOnce(
                         [](UpdateScheduler::OnFinishedCallback on_finished,
                            update_client::Error /*error*/) {
                           std::move(on_finished).Run();
                         },
                         std::move(on_finished)),
                     base::TimeTicks::Now()));
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
  VLOG(1) << "Update completed with error " << static_cast<int>(error);

  UMA_HISTOGRAM_BOOLEAN("ComponentUpdater.UpdateCompleteResult",
                        error != update_client::Error::NONE);
  UMA_HISTOGRAM_ENUMERATION("ComponentUpdater.UpdateCompleteError", error,
                            update_client::Error::MAX_VALUE);
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
}

///////////////////////////////////////////////////////////////////////////////

// The component update factory. Using the component updater as a singleton
// is the job of the browser process.
std::unique_ptr<ComponentUpdateService> ComponentUpdateServiceFactory(
    scoped_refptr<Configurator> config,
    std::unique_ptr<UpdateScheduler> scheduler,
    const std::string& brand) {
  CHECK(config);
  CHECK(scheduler);
  auto update_client = update_client::UpdateClientFactory(config);
  return std::make_unique<CrxUpdateService>(config, std::move(scheduler),
                                            std::move(update_client), brand);
}

// Register prefs required by the component update service.
void RegisterComponentUpdateServicePrefs(PrefRegistrySimple* registry) {
  // If the preference is not set the component updates are enabled by default
  // unless in Chrome for Testing where we never want components to be updated
  // automatically.
  constexpr bool kComponentUpdatesEnabledByDefault =
      !BUILDFLAG(CHROME_FOR_TESTING);

  registry->RegisterBooleanPref(prefs::kComponentUpdatesEnabled,
                                kComponentUpdatesEnabledByDefault);
}

}  // namespace component_updater
