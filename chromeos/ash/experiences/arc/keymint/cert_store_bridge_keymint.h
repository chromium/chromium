// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_KEYMINT_CERT_STORE_BRIDGE_KEYMINT_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_KEYMINT_CERT_STORE_BRIDGE_KEYMINT_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/keymint/public/mojom/cert_store.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/invitation.h"

namespace content {

class BrowserContext;

}  // namespace content

namespace arc::keymint {

// The "keymint" in file name was added to avoid a build conflict with the
// cert_store_bridge.(cc|h) under keymaster. The class name also has "KeyMint"
// to match the file name. When Keymaster support is removed, the "keymint" here
// may be removed.
class CertStoreBridgeKeyMint {
 public:
  explicit CertStoreBridgeKeyMint(content::BrowserContext* context);
  CertStoreBridgeKeyMint(const CertStoreBridgeKeyMint&) = delete;
  CertStoreBridgeKeyMint& operator=(const CertStoreBridgeKeyMint&) = delete;
  virtual ~CertStoreBridgeKeyMint();

  // Attaches a new message pipe to the invitation and binds it to the cert
  // store instance proxy.
  void BindToInvitation(mojo::OutgoingInvitation* invitation);

  bool IsProxyBound() const;

  // Updates the latest information about Chrome OS keys to arc-keymintd.
  void UpdatePlaceholderKeysInKeyMint(
      std::vector<mojom::ChromeOsKeyPtr> keys,
      mojom::CertStoreInstance::UpdatePlaceholderKeysCallback callback);

  // Sets the ARCVM Serial number from Chrome to arc-keymintd.
  // If the proxy is not bound, an error would be logged on the ChromeOS side.
  virtual void SetSerialNumber(const std::string& serial_number);

 private:
  // Points to a proxy bound to the implementation in arc-keymintd.
  mojo::Remote<keymint::mojom::CertStoreInstance> cert_store_proxy_;

  base::WeakPtrFactory<CertStoreBridgeKeyMint> weak_ptr_factory_;
};

}  // namespace arc::keymint

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_KEYMINT_CERT_STORE_BRIDGE_KEYMINT_H_
