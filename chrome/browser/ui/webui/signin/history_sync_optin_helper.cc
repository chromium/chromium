// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin_helper.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker_impl.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capability_fetcher.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"

HistorySyncOptinHelper::HistorySyncOptinHelper(
    signin::IdentityManager* identity_manager,
    const AccountInfo& account_info,
    Delegate* delegate)
    : delegate_(delegate),
      account_enterprise_policy_capability_fetcher_(std::make_unique<
                                                    AccountCapabilityFetcher>(
          identity_manager,
          account_info,
          /*get_capability_state_callback=*/
          base::BindRepeating(
              &HistorySyncOptinHelper::CanApplyAccountLevelEnterprisePolicies,
              base::Unretained(this)),
          /*on_capability_fetched_callback=*/
          base::BindOnce(
              &HistorySyncOptinHelper::MaybeShowHistorySyncOptinScreen,
              base::Unretained(this)))) {
  CHECK(base::FeatureList::IsEnabled(switches::kEnableHistorySyncOptin));
  CHECK(delegate);
}

HistorySyncOptinHelper::~HistorySyncOptinHelper() = default;

void HistorySyncOptinHelper::StartHistorySyncOptinFlow() {
  account_enterprise_policy_capability_fetcher_->FetchCapability();
}

void HistorySyncOptinHelper::MaybeShowHistorySyncOptinScreen(
    signin::Tribool is_managed_account) {
  if (is_managed_account != signin::Tribool::kTrue) {
    // TODO(crbug.com/434964019): Handle the managed account case by showing the
    // corresponding screen first. Deciding on the right screen might
    // require knowledge of the Sync Service's status.
    ShowHistorySyncOptinScreen();
  }
}

void HistorySyncOptinHelper::ShowHistorySyncOptinScreen() {
  delegate_->ShowHistorySyncOptinScreen();
}

signin::Tribool HistorySyncOptinHelper::CanApplyAccountLevelEnterprisePolicies(
    const AccountInfo& account_info) {
  if (!account_info.IsEmpty()) {
    return account_info.CanApplyAccountLevelEnterprisePolicies();
  }
  return signin::Tribool::kUnknown;
}
