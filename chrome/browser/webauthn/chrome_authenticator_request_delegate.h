// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_H_
#define CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"

class Profile;

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace device {
class FidoAuthenticator;
class FidoDiscoveryFactory;
}

class ChromeAuthenticatorRequestDelegate
    : public content::AuthenticatorRequestClientDelegate,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
#if defined(OS_MAC)
  static TouchIdAuthenticatorConfig TouchIdAuthenticatorConfigForProfile(
      Profile* profile);
#endif  // defined(OS_MAC)

  // The |render_frame_host| must outlive this instance.
  explicit ChromeAuthenticatorRequestDelegate(
      content::RenderFrameHost* render_frame_host);
  ~ChromeAuthenticatorRequestDelegate() override;

#if defined(OS_MAC)
  base::Optional<TouchIdAuthenticatorConfig> GetTouchIdAuthenticatorConfig()
      override;
#endif  // defined(OS_MAC)

  base::WeakPtr<ChromeAuthenticatorRequestDelegate> AsWeakPtr();

  AuthenticatorRequestDialogModel* WeakDialogModelForTesting() const;

  // content::AuthenticatorRequestClientDelegate:
  base::Optional<std::string> MaybeGetRelyingPartyIdOverride(
      const std::string& claimed_relying_party_id,
      const url::Origin& caller_origin) override;
  void SetRelyingPartyId(const std::string& rp_id) override;
  bool DoesBlockRequestOnFailure(InterestingFailureReason reason) override;
  void RegisterActionCallbacks(
      base::OnceClosure cancel_callback,
      base::RepeatingClosure start_over_callback,
      device::FidoRequestHandlerBase::RequestCallback request_callback,
      base::RepeatingClosure bluetooth_adapter_power_on_callback) override;
  bool ShouldPermitIndividualAttestation(
      const std::string& relying_party_id) override;
  void ShouldReturnAttestation(
      const std::string& relying_party_id,
      const device::FidoAuthenticator* authenticator,
      bool is_enterprise_attestation,
      base::OnceCallback<void(bool)> callback) override;
  bool SupportsResidentKeys() override;
  void ConfigureCable(
      const url::Origin& origin,
      base::span<const device::CableDiscoveryData> pairings_from_extension,
      device::FidoDiscoveryFactory* discovery_factory) override;
  void SelectAccount(
      std::vector<device::AuthenticatorGetAssertionResponse> responses,
      base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
          callback) override;
  bool IsFocused() override;
  void UpdateLastTransportUsed(
      device::FidoTransportProtocol transport) override;
  void DisableUI() override;
  bool IsWebAuthnUIEnabled() override;

  // device::FidoRequestHandlerBase::Observer:
  void OnTransportAvailabilityEnumerated(
      device::FidoRequestHandlerBase::TransportAvailabilityInfo data) override;
  bool EmbedderControlsAuthenticatorDispatch(
      const device::FidoAuthenticator& authenticator) override;
  void FidoAuthenticatorAdded(
      const device::FidoAuthenticator& authenticator) override;
  void FidoAuthenticatorRemoved(base::StringPiece authenticator_id) override;
  void BluetoothAdapterPowerChanged(bool is_powered_on) override;
  bool SupportsPIN() const override;
  void CollectPIN(
      base::Optional<int> attempts,
      base::OnceCallback<void(std::string)> provide_pin_cb) override;
  void StartBioEnrollment(base::OnceClosure next_callback) override;
  void OnSampleCollected(int bio_samples_remaining) override;
  void FinishCollectToken() override;
  void OnRetryUserVerification(int attempts) override;
  void OnInternalUserVerificationLocked() override;
  void SetMightCreateResidentCredential(bool v) override;

  // AuthenticatorRequestDialogModel::Observer:
  void OnStartOver() override;
  void OnModelDestroyed() override;
  void OnCancelRequest() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeAuthenticatorRequestDelegateTest,
                           TestTransportPrefType);
  FRIEND_TEST_ALL_PREFIXES(ChromeAuthenticatorRequestDelegateTest,
                           TestPairedDeviceAddressPreference);

  content::RenderFrameHost* render_frame_host() const {
    return render_frame_host_;
  }
  content::BrowserContext* browser_context() const;

  base::Optional<device::FidoTransportProtocol> GetLastTransportUsed() const;

  // ShouldPermitCableExtension returns true if the given |origin| may set a
  // caBLE extension. This extension contains website-chosen BLE pairing
  // information that will be broadcast by the device.
  bool ShouldPermitCableExtension(const url::Origin& origin);

  // GetCablePairings returns any known caBLE pairing data.
  virtual std::vector<std::unique_ptr<device::cablev2::Pairing>>
  GetCablePairings();

  void StoreNewCablePairingInPrefs(
      std::unique_ptr<device::cablev2::Pairing> pairing);

  content::RenderFrameHost* const render_frame_host_;
  // Holds ownership of AuthenticatorRequestDialogModel until
  // OnTransportAvailabilityEnumerated() is invoked, at which point the
  // ownership of the model is transferred to AuthenticatorRequestDialogView and
  // |this| instead holds weak pointer of the model via above
  // |weak_dialog_model_|.
  std::unique_ptr<AuthenticatorRequestDialogModel>
      transient_dialog_model_holder_;
  AuthenticatorRequestDialogModel* weak_dialog_model_ = nullptr;
  base::OnceClosure cancel_callback_;
  base::RepeatingClosure start_over_callback_;
  device::FidoRequestHandlerBase::RequestCallback request_callback_;

  // If in the TransportAvailabilityInfo reported by the request handler,
  // disable_embedder_ui is set, this will be set to true. No UI must be
  // rendered and all request handler callbacks will be ignored.
  bool disable_ui_ = false;

  base::WeakPtrFactory<ChromeAuthenticatorRequestDelegate> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(ChromeAuthenticatorRequestDelegate);
};

#endif  // CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_H_
