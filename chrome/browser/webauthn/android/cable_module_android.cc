// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/android/cable_module_android.h"

#include <variant>

#include "base/android/jni_array.h"
#include "base/base64.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/webauthn/android/cable_registration_state.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "content/public/browser/browser_thread.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/cable/v2_registration.h"
#include "device/fido/public/features.h"

// These "headers" actually contains function definitions and thus can only be
// included once across Chromium.
#include "chrome/browser/webauthn/android/jni_headers/CableAuthenticatorModuleProvider_jni.h"

using device::cablev2::authenticator::Registration;

namespace webauthn {
namespace authenticator {

namespace {

const char kSerializedPaaskFieldsName[] = "webauthn.authenticator_info";

const char kWorkProfilePrefName[] = "webauthn.in_work_profile";
// kWorkProfilePrefName wants to be a tristate. Since there's no support for
// that in `PrefService`, it's simulated with a string that is empty if unset,
// and takes one of the following values when set.
const char kInWorkProfile[] = "1";
const char kNotInWorkProfile[] = "0";

// SystemInterface connects a `RegistrationState` to the rest of the system.
// This object is owned by the `RegistrationState`, and that is a singleton
// object. So this object is a singleton too and so can do things like pass a
// pointer to itself to Java functions to route the eventual callback.
class SystemInterface : public RegistrationState::SystemInterface {
 public:
  std::unique_ptr<device::cablev2::authenticator::Registration> NewRegistration(
      device::cablev2::authenticator::Registration::Type type,
      base::OnceCallback<void()> on_ready,
      base::RepeatingCallback<void(
          std::unique_ptr<device::cablev2::authenticator::Registration::Event>)>
          event_callback) override {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    return device::cablev2::authenticator::Register(
        GetDriver(), type, std::move(on_ready), std::move(event_callback));
  }

  void CanDeviceSupportCable(base::OnceCallback<void(bool)> callback) override {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            &SystemInterface::GetCanDeviceSupportCableOnBackgroundSequence),
        std::move(callback));
  }

  void AmInWorkProfile(base::OnceCallback<void(bool)> callback) override {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

    // Checking whether an app is in a work profile is costly. We assume that a
    // given Chrome profile never moves between being in a work profile or not
    // and thus cache the result on disk.
    const std::string work_profile_state =
        g_browser_process->local_state()->GetString(kWorkProfilePrefName);
    if (work_profile_state == kInWorkProfile) {
      std::move(callback).Run(true);
    } else if (work_profile_state == kNotInWorkProfile) {
      std::move(callback).Run(false);
    } else {
      work_profile_callback_ = std::move(callback);
      // Checking whether this Chrome is in a work profile is sufficiently
      // expensive that doing it at startup impacts benchmarks. (See
      // crbug.com/40919627.) Since startup is an especially contended time, we
      // wait a few minutes before doing this check.
      content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
          ->PostDelayedTask(
              FROM_HERE,
              base::BindOnce(&SystemInterface::GetWorkProfileStatus,
                             base::Unretained(this)),
              base::Minutes(3));
    }
  }

  void GetPrelinkFromPlayServices(
      base::OnceCallback<void(std::optional<std::vector<uint8_t>>)> callback)
      override {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(!prelink_callback_);
    prelink_callback_ = std::move(callback);
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            &SystemInterface::GetPrelinkFromPlayServicesOnBackgroundSequence,
            // Passing this pointer is reasonable because this object is owned
            // by a singleton.
            reinterpret_cast<uintptr_t>(this)));
  }

  void RefreshLocalDeviceInfo() override {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DeviceInfoSyncServiceFactory::GetForProfile(
        ProfileManager::GetPrimaryUserProfile())
        ->RefreshLocalDeviceInfo();
  }

  // Called when the Java code has finished getting linking information from
  // Play Services.
  void OnHavePlayServicesLinkingInformation(
      std::optional<std::vector<uint8_t>> cbor) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    std::move(prelink_callback_).Run(std::move(cbor));
  }

  // Called when the Java code has finished checking if we're running in a work
  // profile.
  void OnHaveWorkProfileResult(bool in_work_profile) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

    g_browser_process->local_state()->SetString(
        kWorkProfilePrefName,
        in_work_profile ? kInWorkProfile : kNotInWorkProfile);

    std::move(work_profile_callback_).Run(in_work_profile);
  }

 private:
  static instance_id::InstanceIDDriver* GetDriver() {
    return instance_id::InstanceIDProfileServiceFactory::GetForProfile(
               g_browser_process->profile_manager()->GetPrimaryUserProfile())
        ->driver();
  }

  static bool GetCanDeviceSupportCableOnBackgroundSequence() {
    // This runs on a worker thread because this Java function can take a
    // little while and it shouldn't block the UI thread.
    return Java_CableAuthenticatorModuleProvider_canDeviceSupportCable(
        base::android::AttachCurrentThread());
  }

  static void GetPrelinkFromPlayServicesOnBackgroundSequence(
      uintptr_t this_pointer) {
    // This runs on a worker thread because this Java function can take a
    // little while and it shouldn't block the UI thread.
    Java_CableAuthenticatorModuleProvider_getLinkingInformation(
        base::android::AttachCurrentThread(), this_pointer);
  }

  void GetWorkProfileStatus() {
    // This Java function must run on the UI thread, but that's ok because it
    // defers work to a worker thread itself. It returns its result by calling
    // `OnHaveWorkProfileResult` on this object.
    Java_CableAuthenticatorModuleProvider_amInWorkProfile(
        base::android::AttachCurrentThread(),
        reinterpret_cast<uintptr_t>(this));
  }

  base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>
      prelink_callback_;
  base::OnceCallback<void(bool)> work_profile_callback_;
};

RegistrationState* GetRegistrationState() {
  static base::NoDestructor<RegistrationState> state(
      std::make_unique<SystemInterface>());
  return state.get();
}

syncer::DeviceInfo::PhoneAsASecurityKeyInfo::StatusOrInfo
GetSyncDataIfRegisteredInternal() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  RegistrationState* state = GetRegistrationState();
  if (!state->have_data_for_sync()) {
    // Not yet ready to provide sync data. When the data is ready,
    // |state| will signal to Sync that something changed and this
    // function will be called again.
    state->SignalSyncWhenReady();
    return syncer::DeviceInfo::PhoneAsASecurityKeyInfo::NotReady();
  }

  if (state->am_in_work_profile()) {
    // Never publish pre-linking information when in a work profile, instead
    // route hybrid requests into the main profile.
    return syncer::DeviceInfo::PhoneAsASecurityKeyInfo::NoSupport();
  }

  if (state->link_data_from_play_services()) {
    std::optional<syncer::DeviceInfo::PhoneAsASecurityKeyInfo> paask_info =
        internal::PaaskInfoFromCBOR(*state->link_data_from_play_services());
    if (paask_info) {
      return *paask_info;
    } else {
      LOG(ERROR)
          << "Failed to parse PaaSK prelink information from Play Services";
    }
  }

  return syncer::DeviceInfo::PhoneAsASecurityKeyInfo::NoSupport();
}

void SetPrefIfDifferent(PrefService* state,
                        const char* pref_name,
                        const std::string& value) {
  const std::string existing_value = state->GetString(pref_name);
  if (existing_value != value) {
    state->SetString(pref_name, value);
  }
}

}  // namespace

namespace internal {

std::optional<syncer::DeviceInfo::PhoneAsASecurityKeyInfo> PaaskInfoFromCBOR(
    base::span<const uint8_t> cbor) {
  std::optional<cbor::Value> value = cbor::Reader::Read(cbor);
  if (!value || !value->is_map()) {
    return std::nullopt;
  }
  const cbor::Value::MapValue& map = value->GetMap();

  syncer::DeviceInfo::PhoneAsASecurityKeyInfo paask_info;
  paask_info.tunnel_server_domain = device::cablev2::kTunnelServer.value();

  const auto contact_id_it = map.find(cbor::Value(1));
  if (contact_id_it == map.end() || !contact_id_it->second.is_bytestring()) {
    return std::nullopt;
  }
  paask_info.contact_id = contact_id_it->second.GetBytestring();

  const auto pairing_id_it = map.find(cbor::Value(2));
  if (pairing_id_it == map.end() || !pairing_id_it->second.is_bytestring()) {
    return std::nullopt;
  }
  const auto& pairing_id_bytes = pairing_id_it->second.GetBytestring();
  uint64_t pairing_id;
  if (pairing_id_bytes.size() != sizeof(pairing_id)) {
    return std::nullopt;
  }
  base::byte_span_from_ref(pairing_id).copy_from(pairing_id_bytes);
  if (pairing_id > std::numeric_limits<uint32_t>::max()) {
    return std::nullopt;
  }
  paask_info.id = static_cast<uint32_t>(pairing_id);

  const auto secret_it = map.find(cbor::Value(3));
  if (secret_it == map.end() || !secret_it->second.is_bytestring()) {
    return std::nullopt;
  }
  const auto& secret_bytes = secret_it->second.GetBytestring();
  if (secret_bytes.size() != paask_info.secret.size()) {
    return std::nullopt;
  }
  base::span(paask_info.secret).copy_from(secret_bytes);

  const auto peer_public_key_it = map.find(cbor::Value(4));
  if (peer_public_key_it == map.end() ||
      !peer_public_key_it->second.is_bytestring()) {
    return std::nullopt;
  }
  const auto& peer_public_key_bytes =
      peer_public_key_it->second.GetBytestring();
  if (peer_public_key_bytes.size() != paask_info.peer_public_key_x962.size()) {
    return std::nullopt;
  }
  base::span(paask_info.peer_public_key_x962).copy_from(peer_public_key_bytes);

  return paask_info;
}

std::vector<uint8_t> CBORFromPaaskInfo(
    const syncer::DeviceInfo::PhoneAsASecurityKeyInfo& paask_info) {
  cbor::Value::MapValue map;
  map.emplace(1, paask_info.contact_id);
  const uint64_t pairing_id = paask_info.id;
  map.emplace(2, base::byte_span_from_ref(pairing_id));
  map.emplace(3, paask_info.secret);
  map.emplace(4, paask_info.peer_public_key_x962);
  return cbor::Writer::Write(cbor::Value(std::move(map))).value();
}

syncer::DeviceInfo::PhoneAsASecurityKeyInfo::StatusOrInfo CacheResult(
    syncer::DeviceInfo::PhoneAsASecurityKeyInfo::StatusOrInfo result,
    PrefService* state) {
  // kNoSupportString indicates that there is no support for PaaSK. It is
  // distinct from all base64-encoded values so is distinguishable from an
  // encoded `PhoneAsASecurityKeyInfo`.
  constexpr char kNoSupportString[] = ",";

  if (std::get_if<syncer::DeviceInfo::PhoneAsASecurityKeyInfo::NotReady>(
          &result)) {
    const std::string previous_result_serialized_b64 =
        state->GetString(kSerializedPaaskFieldsName);
    if (previous_result_serialized_b64 == kNoSupportString) {
      return syncer::DeviceInfo::PhoneAsASecurityKeyInfo::NoSupport();
    }

    std::string previous_result_serialized;
    if (previous_result_serialized_b64.empty() ||
        !base::Base64Decode(previous_result_serialized_b64,
                            &previous_result_serialized)) {
      return result;
    }

    std::optional<syncer::DeviceInfo::PhoneAsASecurityKeyInfo> paask_info =
        internal::PaaskInfoFromCBOR(
            base::as_byte_span(previous_result_serialized));
    if (!paask_info) {
      return result;
    }
    return *paask_info;
  } else if (auto* paask_info =
                 std::get_if<syncer::DeviceInfo::PhoneAsASecurityKeyInfo>(
                     &result)) {
    SetPrefIfDifferent(
        state, kSerializedPaaskFieldsName,
        base::Base64Encode(internal::CBORFromPaaskInfo(*paask_info)));
    return result;
  } else if (std::get_if<
                 syncer::DeviceInfo::PhoneAsASecurityKeyInfo::NoSupport>(
                 &result)) {
    SetPrefIfDifferent(state, kSerializedPaaskFieldsName, kNoSupportString);
    return result;
  }

  NOTREACHED();
}

}  // namespace internal

void RegisterForCloudMessages() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  GetRegistrationState()->Register();
}

void RegisterLocalState(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kSerializedPaaskFieldsName, std::string());
  registry->RegisterStringPref(kWorkProfilePrefName, std::string());
}

syncer::DeviceInfo::PhoneAsASecurityKeyInfo::StatusOrInfo
GetSyncDataIfRegistered() {
  return internal::CacheResult(GetSyncDataIfRegisteredInternal(),
                               g_browser_process->local_state());
}

}  // namespace authenticator
}  // namespace webauthn

using webauthn::authenticator::SystemInterface;

// JNI callbacks.

static void JNI_CableAuthenticatorModuleProvider_OnHaveLinkingInformation(
    JNIEnv* env,
    int64_t system_interface_pointer,
    const base::android::JavaRef<jbyteArray>& cbor_java) {
  std::optional<std::vector<uint8_t>> optional_cbor;

  if (cbor_java) {
    std::vector<uint8_t> cbor;
    base::android::JavaByteArrayToByteVector(env, cbor_java, &cbor);
    optional_cbor = std::move(cbor);
  }

  content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&SystemInterface::OnHavePlayServicesLinkingInformation,
                         base::Unretained(reinterpret_cast<SystemInterface*>(
                             static_cast<uintptr_t>(system_interface_pointer))),
                         std::move(optional_cbor)));
}

static void JNI_CableAuthenticatorModuleProvider_OnHaveWorkProfileResult(
    JNIEnv* env,
    int64_t system_interface_pointer,
    bool in_work_profile) {
  content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&SystemInterface::OnHaveWorkProfileResult,
                         base::Unretained(reinterpret_cast<SystemInterface*>(
                             static_cast<uintptr_t>(system_interface_pointer))),
                         in_work_profile));
}

DEFINE_JNI(CableAuthenticatorModuleProvider)
