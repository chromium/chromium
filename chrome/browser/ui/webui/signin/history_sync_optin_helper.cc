// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin_helper.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "components/signin/public/identity_manager/account_capability_fetcher.h"
#include "components/signin/public/identity_manager/tribool.h"

EnterprisePolicyCapabilityObserver::EnterprisePolicyCapabilityObserver(
    signin::IdentityManager* identity_manager,
    const AccountInfo& account_info,
    base::OnceCallback<void(signin::Tribool)>
        on_enterprise_policy_eligibility_fetched_callback)
    : account_enterprise_policy_capability_fetcher_(
          std::make_unique<AccountCapabilityFetcher>(
              identity_manager,
              account_info,
              base::BindRepeating(&EnterprisePolicyCapabilityObserver::
                                      CanApplyAccountLevelEnterprisePolicies,
                                  base::Unretained(this)),
              std::move(on_enterprise_policy_eligibility_fetched_callback))) {}

EnterprisePolicyCapabilityObserver::~EnterprisePolicyCapabilityObserver() =
    default;

void EnterprisePolicyCapabilityObserver::FetchCapability() {
  account_enterprise_policy_capability_fetcher_->FetchCapability();
}

signin::Tribool
EnterprisePolicyCapabilityObserver::CanApplyAccountLevelEnterprisePolicies(
    const AccountInfo& account_info) {
  if (!account_info.IsEmpty()) {
    return account_info.CanApplyAccountLevelEnterprisePolicies();
  }
  return signin::Tribool::kUnknown;
}
