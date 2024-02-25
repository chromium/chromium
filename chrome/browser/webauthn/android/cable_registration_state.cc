// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/android/cable_registration_state.h"

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "crypto/random.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/features.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"

using device::cablev2::authenticator::Registration;

namespace webauthn::authenticator {

RegistrationState::SystemInterface::~SystemInterface() = default;

RegistrationState::RegistrationState(std::unique_ptr<SystemInterface> interface)
    : interface_(std::move(interface)) {}

RegistrationState::~RegistrationState() = default;

void RegistrationState::Register() {
  DCHECK(!linking_registration_);
  DCHECK(!sync_registration_);

  linking_registration_ = interface_->NewRegistration(
      device::cablev2::authenticator::Registration::Type::LINKING,
      base::BindOnce(&RegistrationState::OnLinkingRegistrationReady,
                     base::Unretained(this)),
      base::BindRepeating(&RegistrationState::OnEvent, base::Unretained(this)));
  sync_registration_ = interface_->NewRegistration(
      device::cablev2::authenticator::Registration::Type::SYNC,
      base::BindOnce(&RegistrationState::OnSyncRegistrationReady,
                     base::Unretained(this)),
      base::BindRepeating(&RegistrationState::OnEvent, base::Unretained(this)));
  interface_->CanDeviceSupportCable(base::BindOnce(
      &RegistrationState::OnDeviceSupportResult, base::Unretained(this)));
  interface_->AmInWorkProfile(base::BindOnce(
      &RegistrationState::OnWorkProfileResult, base::Unretained(this)));

  std::string secret_base64 = interface_->GetRootSecret();

  if (!secret_base64.empty()) {
    std::string secret_str;
    if (base::Base64Decode(secret_base64, &secret_str) &&
        secret_str.size() == secret_.size()) {
      memcpy(secret_.data(), secret_str.data(), secret_.size());
    } else {
      secret_base64.clear();
    }
  }

  if (secret_base64.empty()) {
    crypto::RandBytes(secret_);
    interface_->SetRootSecret(base::Base64Encode(secret_));
  }

  interface_->CalculateIdentityKey(
      secret_, base::BindOnce(&RegistrationState::OnIdentityKeyReady,
                              base::Unretained(this)));
}

// have_data_for_sync returns true if this object has loaded enough state to
// put information into sync's DeviceInfo.
bool RegistrationState::have_data_for_sync() const {
  return device_supports_cable_.has_value() && identity_key_ &&
         am_in_work_profile_.has_value() && sync_registration_ != nullptr &&
         sync_registration_->contact_id() && have_play_services_data();
}

void RegistrationState::SignalSyncWhenReady() {
  if (sync_registration_ && !sync_registration_->contact_id()) {
    sync_registration_->PrepareContactID();
  }
  if (!have_play_services_data() && !play_services_query_pending_) {
    QueryPlayServices();
  }
  signal_sync_when_ready_ = true;
}

bool RegistrationState::have_play_services_data() const {
  // If there's no result, then we're not ready.
  if (!have_link_data_from_play_services_) {
    return false;
  }
  // If there's a query already pending then the result must be stale and
  // there's nothing more to do here.
  if (play_services_query_pending_) {
    return false;
  }
  const base::TimeDelta staleness =
      base::TimeTicks::Now() - link_data_from_play_services_timeticks_;
  return staleness < base::Hours(12);
}

void RegistrationState::QueryPlayServices() {
  DCHECK(!play_services_query_pending_);

  play_services_query_pending_ = true;
  interface_->GetPrelinkFromPlayServices(
      base::BindOnce(&RegistrationState::OnHavePlayServicesLinkingInformation,
                     base::Unretained(this)));
}

void RegistrationState::OnHavePlayServicesLinkingInformation(
    std::optional<std::vector<uint8_t>> cbor) {
  DCHECK(play_services_query_pending_);

  play_services_query_pending_ = false;
  link_data_from_play_services_ = std::move(cbor);
  have_link_data_from_play_services_ = true;
  link_data_from_play_services_timeticks_ = base::TimeTicks::Now();
  MaybeSignalSync();
}

void RegistrationState::OnLinkingRegistrationReady() {
  MaybeFlushPendingEvent();
}

void RegistrationState::OnSyncRegistrationReady() {
  MaybeSignalSync();
}

void RegistrationState::OnEvent(std::unique_ptr<Registration::Event> event) {
  pending_event_ = std::move(event);

  MaybeFlushPendingEvent();
}

void RegistrationState::MaybeFlushPendingEvent() {
  if (!pending_event_) {
    return;
  }

  if (pending_event_->source == Registration::Type::LINKING &&
      !pending_event_->contact_id) {
    // This GCM message is from a QR-linked peer so it needs the contact ID
    // to be processed.
    pending_event_->contact_id = linking_registration_->contact_id();

    if (!pending_event_->contact_id) {
      // The contact ID isn't ready yet. Wait until it is.
      linking_registration_->PrepareContactID();
      return;
    }
  }

  std::unique_ptr<Registration::Event> event(std::move(pending_event_));
  if (event->source == Registration::Type::SYNC) {
    // If this is from a synced peer then we limit how old the keys can be.
    // Clank will update its device information once per day (when launched)
    // and we piggyback on that to transmit fresh keys. Therefore syncing
    // peers should have reasonably recent information.
    uint64_t id;
    static_assert(EXTENT(event->pairing_id) == sizeof(id), "");
    memcpy(&id, event->pairing_id.data(), sizeof(id));

    // A maximum age is enforced for sync secrets so that any leak of
    // information isn't valid forever. The desktop ignores DeviceInfo
    // records with information that is too old so this should never happen
    // with honest clients.
    if (id > std::numeric_limits<uint32_t>::max() ||
        device::cablev2::sync::IDIsMoreThanNPeriodsOld(
            static_cast<uint32_t>(id),
            device::cablev2::kMaxSyncInfoDaysForProducer)) {
      return;
    }
  }

  const std::optional<std::vector<uint8_t>> serialized(event->Serialize());
  if (!serialized) {
    return;
  }

  interface_->OnCloudMessage(
      std::move(*serialized),
      event->request_type == device::FidoRequestType::kMakeCredential);
}

void RegistrationState::MaybeSignalSync() {
  if (!signal_sync_when_ready_ || !have_data_for_sync()) {
    return;
  }
  signal_sync_when_ready_ = false;

  interface_->RefreshLocalDeviceInfo();
}

void RegistrationState::OnDeviceSupportResult(bool result) {
  device_supports_cable_ = result;
  MaybeSignalSync();
}

void RegistrationState::OnWorkProfileResult(bool result) {
  am_in_work_profile_ = result;
  MaybeSignalSync();
}

void RegistrationState::OnIdentityKeyReady(
    bssl::UniquePtr<EC_KEY> identity_key) {
  identity_key_ = std::move(identity_key);
  MaybeSignalSync();
}

}  // namespace webauthn::authenticator
