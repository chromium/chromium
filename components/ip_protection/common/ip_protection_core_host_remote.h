// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CORE_HOST_REMOTE_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CORE_HOST_REMOTE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "components/ip_protection/mojom/core.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ip_protection {

// A simple ref-counted wrapper around `ip_protection::mojom::CoreHost`.
class IpProtectionCoreHostRemote
    : public base::RefCounted<IpProtectionCoreHostRemote> {
 public:
  explicit IpProtectionCoreHostRemote(
      mojo::PendingRemote<ip_protection::mojom::CoreHost> core_host);

  mojo::Remote<ip_protection::mojom::CoreHost>& core_host() {
    return core_host_;
  }

 protected:
  friend class base::RefCounted<IpProtectionCoreHostRemote>;
  virtual ~IpProtectionCoreHostRemote();

 private:
  mojo::Remote<ip_protection::mojom::CoreHost> core_host_;
};
}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CORE_HOST_REMOTE_H_
