// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/components/dbus/hermes/constants.h"
#include "chromeos/ash/components/dbus/hermes/fake_hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_response_status.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/bus.h"
#include "dbus/dbus_result.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace ash {
namespace {

HermesEuiccClient* g_instance = nullptr;

// Activation codes with preceding/trailing whitespace can cause issues when
// provided to Hermes; for example, an SM-DS activation code with trailing
// whitespace may result in profiles not being discovered. This function trims
// this whitespace. This function returns a `std::string` since
// `dbus::MessageWriter::AppendString()` does not respect the length of
// `std::string_view` if it is shorter than the string it is a substring of.
std::string PrepareActivationCodeForHermes(const std::string& activation_code) {
  std::string trimmed;
  base::TrimWhitespaceASCII(activation_code, base::TrimPositions::TRIM_ALL,
                            &trimmed);
  return trimmed;
}

void MaybeEmitHermesInstallationAttemptStep(
    HermesEuiccClient::InstallationAttemptStep step,
    size_t attempt) {
  if (attempt > 0) {
    return;
  }
  base::UmaHistogramEnumeration(
      HermesEuiccClient::kHermesInstallationAttemptStepsHistogram, step);
}

}  // namespace

HermesEuiccClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, hermes::kHermesEuiccInterface, callback) {
  RegisterProperty(hermes::euicc::kEidProperty, &eid_);
  RegisterProperty(hermes::euicc::kIsActiveProperty, &is_active_);
  RegisterProperty(hermes::euicc::kProfilesProperty, &profiles_);
  RegisterProperty(hermes::euicc::kPhysicalSlotProperty, &physical_slot_);
}

HermesEuiccClient::Properties::~Properties() = default;

class HermesEuiccClientImpl : public HermesEuiccClient {
 public:
  explicit HermesEuiccClientImpl(dbus::Bus* bus) : bus_(bus) {}
  explicit HermesEuiccClientImpl(const HermesEuiccClient&) = delete;
  ~HermesEuiccClientImpl() override = default;

  using ProxyPropertiesPair =
      std::pair<dbus::ObjectProxy*, std::unique_ptr<Properties>>;
  using ObjectMap = std::map<dbus::ObjectPath, ProxyPropertiesPair>;

  // HermesEuiccClient:
  void InstallProfileFromActivationCode(
      const dbus::ObjectPath& euicc_path,
      const std::string& activation_code,
      const std::string& confirmation_code,
      InstallCarrierProfileCallback callback) override {
    dbus::ObjectProxy* object_proxy = GetOrCreateProperties(euicc_path).first;
    // On managed devices, attempts to install profile could happen right after
    // boot and it results in a dbus error if hermes hasn't started yet. This
    // call waits for hermes to start before attempting installation.
    object_proxy->WaitForServiceToBeAvailable(base::BindOnce(
        &HermesEuiccClientImpl::InstallProfileFromActivationCodeImpl,
        weak_ptr_factory_.GetWeakPtr(), std::move(euicc_path), activation_code,
        confirmation_code, std::move(callback), /*attempt=*/0));
    base::UmaHistogramEnumeration(
        HermesEuiccClient::kHermesInstallationAttemptStepsHistogram,
        InstallationAttemptStep::kInstallationRequested);
  }

  void InstallProfileFromActivationCodeImpl(
      const dbus::ObjectPath& euicc_path,
      const std::string& activation_code,
      const std::string& confirmation_code,
      InstallCarrierProfileCallback callback,
      int attempt,
      bool service_is_available) {
    if (!service_is_available) {
      NET_LOG(ERROR) << "Failed to wait for D-Bus service to become available";
      std::move(callback).Run(HermesResponseStatus::kErrorWrongState,
                              dbus::DBusResult::kErrorServiceUnknown, nullptr);
      MaybeEmitHermesInstallationAttemptStep(
          InstallationAttemptStep::kHermesUnavailable, attempt);
      return;
    }
    dbus::ObjectProxy* object_proxy = GetOrCreateProperties(euicc_path).first;
    dbus::MethodCall method_call(
        hermes::kHermesEuiccInterface,
        hermes::euicc::kInstallProfileFromActivationCode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(PrepareActivationCodeForHermes(activation_code));
    writer.AppendString(confirmation_code);
    object_proxy->CallMethodWithErrorResponse(
        &method_call, hermes_constants::kHermesNetworkOperationTimeoutMs,
        base::BindOnce(&HermesEuiccClientImpl::OnProfileInstallResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(euicc_path),
                       activation_code, confirmation_code, std::move(callback),
                       attempt));
    MaybeEmitHermesInstallationAttemptStep(
        InstallationAttemptStep::kInstallationStarted, attempt);
  }

  void InstallPendingProfile(const dbus::ObjectPath& euicc_path,
                             const dbus::ObjectPath& carrier_profile_path,
                             const std::string& confirmation_code,
                             HermesResponseCallback callback) override {
    dbus::MethodCall method_call(hermes::kHermesEuiccInterface,
                                 hermes::euicc::kInstallPendingProfile);
    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(carrier_profile_path);
    writer.AppendString(confirmation_code);
    dbus::ObjectProxy* object_proxy = GetOrCreateProperties(euicc_path).first;
    object_proxy->CallMethodWithErrorResponse(
        &method_call, hermes_constants::kHermesNetworkOperationTimeoutMs,
        base::BindOnce(&HermesEuiccClientImpl::OnHermesStatusResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RequestPendingProfiles(const dbus::ObjectPath& euicc_path,
                              const std::string& root_smds,
                              HermesResponseCallback callback) override {
    dbus::MethodCall method_call(hermes::kHermesEuiccInterface,
                                 hermes::euicc::kRequestPendingProfiles);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(root_smds);
    dbus::ObjectProxy* object_proxy = GetOrCreateProperties(euicc_path).first;
    object_proxy->CallMethodWithErrorResponse(
        &method_call, hermes_constants::kHermesNetworkOperationTimeoutMs,
        base::BindOnce(&HermesEuiccClientImpl::OnHermesStatusResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RefreshInstalledProfiles(const dbus::ObjectPath& euicc_path,
                                bool restore_slot,
                                HermesResponseCallback callback) override {
    dbus::MethodCall method_call(hermes::kHermesEuiccInterface,
                                 hermes::euicc::kRefreshInstalledProfiles);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(restore_slot);
    dbus::ObjectProxy* object_proxy = GetOrCreateProperties(euicc_path).first;
    object_proxy->CallMethodWithErrorResponse(
        &method_call, hermes_constants::kHermesNetworkOperationTimeoutMs,
        base::BindOnce(&HermesEuiccClientImpl::OnHermesStatusResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RefreshSmdxProfiles(const dbus::ObjectPath& euicc_path,
                           const std::string& activation_code,
                           bool restore_slot,
                           RefreshSmdxProfilesCallback callback) override {
    dbus::MethodCall method_call(hermes::kHermesEuiccInterface,
                                 hermes::euicc::kRefreshSmdxProfiles);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(PrepareActivationCodeForHermes(activation_code));
    writer.AppendBool(restore_slot);
    dbus::ObjectProxy* object_proxy = GetOrCreateProperties(euicc_path).first;
    object_proxy->CallMethodWithErrorResponse(
        &method_call, hermes_constants::kHermesNetworkOperationTimeoutMs,
        base::BindOnce(&HermesEuiccClientImpl::OnRefreshSmdxProfilesResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UninstallProfile(const dbus::ObjectPath& euicc_path,
                        const dbus::ObjectPath& carrier_profile_path,
                        HermesResponseCallback callback) override {
    dbus::MethodCall method_call(hermes::kHermesEuiccInterface,
                                 hermes::euicc::kUninstallProfile);
    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(carrier_profile_path);
    dbus::ObjectProxy* object_proxy = GetOrCreateProperties(euicc_path).first;
    object_proxy->CallMethodWithErrorResponse(
        &method_call, hermes_constants::kHermesNetworkOperationTimeoutMs,
        base::BindOnce(&HermesEuiccClientImpl::OnHermesStatusResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void ResetMemory(const dbus::ObjectPath& euicc_path,
                   hermes::euicc::ResetOptions reset_option,
                   HermesResponseCallback callback) override {
    dbus::MethodCall method_call(hermes::kHermesEuiccInterface,
                                 hermes::euicc::kResetMemory);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(static_cast<int32_t>(reset_option));
    dbus::ObjectProxy* object_proxy = GetOrCreateProperties(euicc_path).first;
    object_proxy->CallMethodWithErrorResponse(
        &method_call, hermes_constants::kHermesOperationTimeoutMs,
        base::BindOnce(&HermesEuiccClientImpl::OnResetMemoryResponse,
                       weak_ptr_factory_.GetWeakPtr(), euicc_path,
                       std::move(callback)));
  }

  Properties* GetProperties(const dbus::ObjectPath& euicc_path) override {
    return GetOrCreateProperties(euicc_path).second.get();
  }

  TestInterface* GetTestInterface() override { return nullptr; }

  HermesEuiccClient& operator=(const HermesEuiccClient&) = delete;

 private:
  const ProxyPropertiesPair& GetOrCreateProperties(
      const dbus::ObjectPath& euicc_path) {
    auto it = object_map_.find(euicc_path);
    if (it != object_map_.end())
      return it->second;

    dbus::ObjectProxy* object_proxy =
        bus_->GetObjectProxy(hermes::kHermesServiceName, euicc_path);

    auto properties = std::make_unique<Properties>(
        object_proxy,
        base::BindRepeating(&HermesEuiccClientImpl::OnPropertyChanged,
                            weak_ptr_factory_.GetWeakPtr(), euicc_path));
    properties->ConnectSignals();
    properties->GetAll();

    object_map_[euicc_path] =
        std::make_pair(object_proxy, std::move(properties));
    return object_map_[euicc_path];
  }

  void OnPropertyChanged(const dbus::ObjectPath& euicc_path,
                         const std::string& property_name) {
    for (auto& observer : observers()) {
      observer.OnEuiccPropertyChanged(euicc_path, property_name);
    }
  }

  void OnProfileInstallResponse(const dbus::ObjectPath& euicc_path,
                                const std::string& activation_code,
                                const std::string& confirmation_code,
                                InstallCarrierProfileCallback callback,
                                int attempt,
                                dbus::Response* response,
                                dbus::ErrorResponse* error_response) {
    if (error_response) {
      NET_LOG(ERROR) << "Profile install failed with error: "
                     << error_response->GetErrorName();
      if (HermesResponseStatusFromErrorName(error_response->GetErrorName()) ==
              HermesResponseStatus::kErrorUnknownResponse &&
          attempt < kMaxInstallAttempts) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(
                &HermesEuiccClientImpl::InstallProfileFromActivationCodeImpl,
                weak_ptr_factory_.GetWeakPtr(), std::move(euicc_path),
                activation_code, confirmation_code, std::move(callback),
                attempt + 1, /*service_is_available=*/true),
            kInstallRetryDelay);
        return;
      }
      std::move(callback).Run(
          HermesResponseStatusFromErrorName(error_response->GetErrorName()),
          GetResult(error_response), nullptr);
      base::UmaHistogramEnumeration(
          HermesEuiccClient::kHermesInstallationAttemptStepsHistogram,
          InstallationAttemptStep::kInstallationFailed);
      return;
    }

    if (!response) {
      // No Error or Response received.
      NET_LOG(ERROR) << "Carrier profile installation Error: No error or "
                        "response received.";
      std::move(callback).Run(HermesResponseStatus::kErrorNoResponse,
                              dbus::DBusResult::kErrorNoReply, nullptr);
      base::UmaHistogramEnumeration(
          HermesEuiccClient::kHermesInstallationAttemptStepsHistogram,
          InstallationAttemptStep::kInstallationNoResponse);
      return;
    }

    dbus::MessageReader reader(response);
    dbus::ObjectPath profile_path;
    reader.PopObjectPath(&profile_path);
    std::move(callback).Run(HermesResponseStatus::kSuccess,
                            dbus::DBusResult::kSuccess, &profile_path);
    base::UmaHistogramEnumeration(
        HermesEuiccClient::kHermesInstallationAttemptStepsHistogram,
        InstallationAttemptStep::kInstallationSucceeded);
  }

  void OnRefreshSmdxProfilesResponse(RefreshSmdxProfilesCallback callback,
                                     dbus::Response* response,
                                     dbus::ErrorResponse* error_response) {
    std::vector<dbus::ObjectPath> profile_paths;

    if (error_response) {
      NET_LOG(ERROR) << "Refresh SM-DX profiles failed with error: "
                     << error_response->GetErrorName();
      std::move(callback).Run(
          HermesResponseStatusFromErrorName(error_response->GetErrorName()),
          profile_paths);
      return;
    }

    if (!response) {
      // No Error or Response received.
      NET_LOG(ERROR) << "Refresh SM-DX profiles Error: No error or "
                        "response received.";
      std::move(callback).Run(HermesResponseStatus::kErrorNoResponse,
                              profile_paths);
      return;
    }

    dbus::MessageReader reader(response);
    reader.PopArrayOfObjectPaths(&profile_paths);
    std::move(callback).Run(HermesResponseStatus::kSuccess, profile_paths);
  }

  void OnHermesStatusResponse(HermesResponseCallback callback,
                              dbus::Response* response,
                              dbus::ErrorResponse* error_response) {
    if (error_response) {
      NET_LOG(ERROR) << "Hermes Euicc operation failed with error: "
                     << error_response->GetErrorName();
      std::move(callback).Run(
          HermesResponseStatusFromErrorName(error_response->GetErrorName()));
      return;
    }
    std::move(callback).Run(HermesResponseStatus::kSuccess);
  }

  void OnResetMemoryResponse(const dbus::ObjectPath& euicc_path,
                             HermesResponseCallback callback,
                             dbus::Response* response,
                             dbus::ErrorResponse* error_response) {
    OnHermesStatusResponse(std::move(callback), response, error_response);

    if (error_response) {
      return;
    }

    for (auto& observer : observers()) {
      observer.OnEuiccReset(euicc_path);
    }
  }

  raw_ptr<dbus::Bus> bus_;
  ObjectMap object_map_;
  base::WeakPtrFactory<HermesEuiccClientImpl> weak_ptr_factory_{this};
};

HermesEuiccClient::HermesEuiccClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

HermesEuiccClient::~HermesEuiccClient() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void HermesEuiccClient::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void HermesEuiccClient::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

// static
void HermesEuiccClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  DCHECK(!g_instance);
  new HermesEuiccClientImpl(bus);
}

// static
void HermesEuiccClient::InitializeFake() {
  new FakeHermesEuiccClient();
}

// static
void HermesEuiccClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
HermesEuiccClient* HermesEuiccClient::Get() {
  return g_instance;
}

}  // namespace ash
