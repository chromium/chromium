// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_ANDROID_CABLE_REGISTRATION_STATE_H_
#define CHROME_BROWSER_WEBAUTHN_ANDROID_CABLE_REGISTRATION_STATE_H_

#include <array>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "device/fido/cable/v2_registration.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace webauthn::authenticator {

// RegistrationState is a singleton object that loads an install-wide secret at
// startup and holds two FCM registrations. One registration, the "linking"
// registration, is used when the user links with another device by scanning a
// QR code. The second is advertised via Sync for other devices signed into the
// same account. The reason for having two registrations is that the linking
// registration can be rotated if the user wishes to unlink all QR-linked
// devices. But we don't want to break synced peers when that happens. Instead,
// for synced peers we require that they have received a recent sync status from
// this device, i.e. we rotate them automatically.
class RegistrationState {
 public:
  // SystemInterface abstracts the rest of the system. This is mocked out for
  // tests.
  class SystemInterface {
   public:
    virtual ~SystemInterface();

    // See v2_registration.h to understand this function.
    virtual std::unique_ptr<device::cablev2::authenticator::Registration>
    NewRegistration(
        device::cablev2::authenticator::Registration::Type type,
        base::OnceCallback<void()> on_ready,
        base::RepeatingCallback<
            void(std::unique_ptr<
                 device::cablev2::authenticator::Registration::Event>)>
            event_callback) = 0;

    // Returns the previous value passed to `SetRootSecret`.
    virtual std::string GetRootSecret() = 0;

    // Persist a short opaque string on disk, such that other applications
    // cannot access it.
    virtual void SetRootSecret(std::string secret) = 0;

    // Test whether the current device is suitable for prelinking.
    virtual void CanDeviceSupportCable(
        base::OnceCallback<void(bool)> callback) = 0;

    // Test whether the current process is an in Android work profile.
    virtual void AmInWorkProfile(base::OnceCallback<void(bool)> callback) = 0;

    // Generate a P-256 key pair from a seed.
    virtual void CalculateIdentityKey(
        const std::array<uint8_t, 32>& secret,
        base::OnceCallback<void(bssl::UniquePtr<EC_KEY>)> callback) = 0;

    // Fetch prelinking information from Play Services, if any.
    virtual void GetPrelinkFromPlayServices(
        base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>
            callback) = 0;

    // Process an FCM message. The `serialized` argument contains a
    // `device::cablev2::authenticator::Event` in serialized form.
    virtual void OnCloudMessage(std::vector<uint8_t> serialized,
                                bool is_make_credential) = 0;

    // Request that Sync refresh the DeviceInfo entity for this device.
    virtual void RefreshLocalDeviceInfo() = 0;
  };

  explicit RegistrationState(std::unique_ptr<SystemInterface> interface);
  ~RegistrationState();

  void Register();

  bool is_registered_for_linking() const {
    return linking_registration_ != nullptr;
  }
  bool is_registered_for_sync() const { return sync_registration_ != nullptr; }
  device::cablev2::authenticator::Registration* linking_registration() const {
    return linking_registration_.get();
  }
  device::cablev2::authenticator::Registration* sync_registration() const {
    return sync_registration_.get();
  }
  const std::array<uint8_t, 32>& secret() const { return secret_; }
  const EC_KEY* identity_key() const { return identity_key_.get(); }
  bool device_supports_cable() const { return *device_supports_cable_; }
  bool am_in_work_profile() const { return *am_in_work_profile_; }
  const std::optional<std::vector<uint8_t>>& link_data_from_play_services()
      const {
    DCHECK(have_link_data_from_play_services_);
    return link_data_from_play_services_;
  }

  // have_data_for_sync returns true if this object has loaded enough state to
  // put information into sync's DeviceInfo.
  bool have_data_for_sync() const;

  // Request that this object trigger a DeviceInfo refresh when
  // `have_data_for_sync` is true.
  void SignalSyncWhenReady();

 private:
  bool have_play_services_data() const;
  void QueryPlayServices();
  void OnHavePlayServicesLinkingInformation(
      std::optional<std::vector<uint8_t>> cbor);

  void OnLinkingRegistrationReady();
  void OnSyncRegistrationReady();

  // OnEvent is called when a GCM message is received.
  void OnEvent(
      std::unique_ptr<device::cablev2::authenticator::Registration::Event>
          event);

  void MaybeFlushPendingEvent();

  // MaybeSignalSync prompts the Sync system to refresh local-device data if
  // the Sync data is now ready and |signal_sync_when_ready_| has been set to
  // indicate that the Sync data was not available last time Sync queried it.
  void MaybeSignalSync();

  // OnCanDeviceSupportCable is run with the result of `TestDeviceSupport`.
  void OnDeviceSupportResult(bool result);

  // OnWorkProfileResult is run with the result of `AmInWorkProfile`.
  void OnWorkProfileResult(bool result);

  // OnIdentityKeyReady is run with the result of `CalculateIdentityKey`.
  void OnIdentityKeyReady(bssl::UniquePtr<EC_KEY> identity_key);

  const std::unique_ptr<SystemInterface> interface_;
  std::unique_ptr<device::cablev2::authenticator::Registration>
      linking_registration_;
  std::unique_ptr<device::cablev2::authenticator::Registration>
      sync_registration_;
  std::array<uint8_t, 32> secret_;
  // identity_key_ is a public/private P-256 key that is calculated from
  // `secret_`. It's cached because it takes some time to compute.
  bssl::UniquePtr<EC_KEY> identity_key_;
  std::unique_ptr<device::cablev2::authenticator::Registration::Event>
      pending_event_;
  // device_supports_cable_ caches the result of a Java function that checks
  // some prerequisites: that the device has Bluetooth and a screenlock. If
  // this value is |nullopt| then its value has not yet been determined.
  //
  // The presence of a screen lock could change but, because of this caching,
  // Clank won't notice in this context until the process restarts. Users can
  // always use a QR code if pre-linking hasn't worked by the time they need
  // it.
  std::optional<bool> device_supports_cable_;
  // am_in_work_profile_ stores whether the current process is in an Android
  // work profile.
  std::optional<bool> am_in_work_profile_;
  // link_data_from_play_services_ contains the response from Play Services, as
  // CBOR-encoded linking information, or `nullopt` if the call was
  // unsuccessful. This field is only meaningful if
  // `have_link_data_from_play_services_` is true.
  std::optional<std::vector<uint8_t>> link_data_from_play_services_;
  // have_link_data_from_play_services_ is true if any call to Play Services has
  // ever completed, successful or not.
  bool have_link_data_from_play_services_ = false;
  // link_data_from_play_services_timeticks_ contains the timestamp when
  // `link_data_from_play_services_` was set.
  base::TimeTicks link_data_from_play_services_timeticks_;
  // play_services_query_pending_ is true if a request to Play Services is
  // currently outstanding.
  bool play_services_query_pending_ = false;
  bool signal_sync_when_ready_ = false;
};

}  // namespace webauthn::authenticator

#endif  // CHROME_BROWSER_WEBAUTHN_ANDROID_CABLE_REGISTRATION_STATE_H_
