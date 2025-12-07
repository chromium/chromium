// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/strike_databases/walletable_pass_save_strike_database_by_host.h"

#include <string>
#include <string_view>

#include "base/strings/string_util.h"

namespace wallet {

namespace {
// Used as a separator to create "(pass_category, host)" pairs.
constexpr char kHostSeparator = ';';
}  // namespace

std::string WalletablePassSaveStrikeDatabaseByHost::GetId(
    std::string_view pass_category,
    std::string_view host) {
  return base::JoinString({pass_category, host},
                          std::string_view(&kHostSeparator, 1));
}

}  // namespace wallet
