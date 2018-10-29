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
}

class ChromeAuthenticatorRequestDelegate
    : public content::AuthenticatorRequestClientDelegate,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
#if defined(OS_MACOSX)
  static TouchIdAuthenticatorConfig TouchIdAuthenticatorConfigForProfile(
      Profile* profile);
#endif  // defined(OS_MACOSX)

  // The |render_frame_host| must outlive this instance.
  explicit ChromeAuthenticatorRequestDelegate(
      content::RenderFrameHost* render_frame_host);
  ~ChromeAuthenticatorRequestDelegate() override;

#if defined(OS_MACOSX)
  base::Optional<TouchIdAuthenticatorConfig> GetTouchIdAuthenticatorConfig()
      const override;
#endif  // defined(OS_MACOSX)

  base::WeakPtr<ChromeAuthenticatorRequestDelegate> AsWeakPtr();

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeAuthenticatorRequestDelegateTest,
                           TestTransportPrefType);
  FRIEND_TEST_ALL_PREFIXES(ChromeAuthenticatorRequestDelegateTest,
                           TestPairedDeviceAddressPreference);

  content::RenderFrameHost* render_frame_host() const {
    return render_frame_host_;
  }
  content::BrowserContext* browser_context() const;

  // content::AuthenticatorRequestClientDelegate:
  void DidFailWithInterestingReason(InterestingFailureReason reason) override;
  void RegisterActionCallbacks(
      base::OnceClosure cancel_callback,
      device::FidoRequestHandlerBase::RequestCallback request_callback,
      base::RepeatingClosure bluetooth_adapter_power_on_callback,
      device::FidoRequestHandlerBase::BlePairingCallback ble_pairing_callback)
      override;
  bool ShouldPermitIndividualAttestation(
      const std::string& relying_party_id) override;
  void ShouldReturnAttestation(
      const std::string& relying_party_id,
      base::OnceCallback<void(bool)> callback) override;
  bool IsFocused() override;
  void UpdateLastTransportUsed(
      device::FidoTransportProtocol transport) override;

  // device::FidoRequestHandlerBase::TransportAvailabilityObserver:
  void OnTransportAvailabilityEnumerated(
      device::FidoRequestHandlerBase::TransportAvailabilityInfo data) override;
  bool EmbedderControlsAuthenticatorDispatch(
      const device::FidoAuthenticator& authenticator) override;
  void FidoAuthenticatorAdded(
      const device::FidoAuthenticator& authenticator) override;
  void FidoAuthenticatorRemoved(base::StringPiece authenticator_id) override;
  void FidoAuthenticatorIdChanged(base::StringPiece old_authenticator_id,
                                  std::string new_authenticator_id) override;
  void FidoAuthenticatorPairingModeChanged(base::StringPiece authenticator_id,
                                           bool is_in_pairing_mode) override;
  void BluetoothAdapterPowerChanged(bool is_powered_on) override;

  // AuthenticatorRequestDialogModel::Observer:
  void OnModelDestroyed() override;
  void OnCancelRequest() override;

  void AddFidoBleDeviceToPairedList(std::string device_address);
  base::Optional<device::FidoTransportProtocol> GetLastTransportUsed() const;
  const base::ListValue* GetPreviouslyPairedFidoBleDeviceAddresses() const;

  content::RenderFrameHost* const render_frame_host_;
  AuthenticatorRequestDialogModel* weak_dialog_model_ = nullptr;
  // Holds ownership of AuthenticatorRequestDialogModel until
  // OnTransportAvailabilityEnumerated() is invoked, at which point the
  // ownership of the model is transferred to AuthenticatorRequestDialogView and
  // |this| instead holds weak pointer of the model via above
  // |weak_dialog_model_|.
  std::unique_ptr<AuthenticatorRequestDialogModel>
      transient_dialog_model_holder_;
  base::OnceClosure cancel_callback_;
  device::FidoRequestHandlerBase::RequestCallback request_callback_;

  base::WeakPtrFactory<ChromeAuthenticatorRequestDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAuthenticatorRequestDelegate);
};

#endif  // CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_H_
