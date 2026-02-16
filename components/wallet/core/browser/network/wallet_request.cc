// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/network/wallet_request.h"

#include "base/values.h"
#include "components/version_info/version_info.h"

namespace wallet {

net::HttpRequestHeaders WalletRequest::GetRequestHeaders() const {
  return net::HttpRequestHeaders();
}

// static
ClientInfo WalletRequest::BuildClientInfo() {
  ClientInfo client_info;
  client_info.mutable_chrome_client_info()->set_version(
      version_info::GetVersionNumber());
  return client_info;
}

}  // namespace wallet
