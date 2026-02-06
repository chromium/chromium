// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/network/wallet_request.h"

#include "base/values.h"
#include "components/version_info/version_info.h"

namespace wallet {

base::DictValue WalletRequest::BuildClientInfo() {
  base::DictValue chrome_client_info =
      base::DictValue().Set("version", version_info::GetVersionNumber());

  return base::DictValue().Set("chrome_client_info",
                               std::move(chrome_client_info));
}

}  // namespace wallet
