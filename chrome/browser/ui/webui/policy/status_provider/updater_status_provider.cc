// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy/status_provider/updater_status_provider.h"

#include <windows.h>

#include <DSRole.h>

#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/google/google_update_policy_fetcher_win.h"
#include "chrome/install_static/install_util.h"

UpdaterStatusProvider::UpdaterStatusProvider() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&UpdaterStatusProvider::FetchActiveDirectoryDomain),
      base::BindOnce(&UpdaterStatusProvider::OnDomainReceived,
                     weak_factory_.GetWeakPtr()));
}

UpdaterStatusProvider::~UpdaterStatusProvider() {}

void UpdaterStatusProvider::SetUpdaterStatus(
    std::unique_ptr<GoogleUpdateState> status) {
  updater_status_ = std::move(status);
  NotifyStatusChange();
}

void UpdaterStatusProvider::GetStatus(base::DictionaryValue* dict) {
  if (!domain_.empty())
    dict->SetStringKey("domain", domain_);
  if (!updater_status_)
    return;
  if (!updater_status_->version.empty())
    dict->SetStringKey("version", base::WideToUTF8(updater_status_->version));
  if (!updater_status_->last_checked_time.is_null()) {
    dict->SetStringKey(
        "timeSinceLastRefresh",
        GetTimeSinceLastActionString(updater_status_->last_checked_time));
  }
}

// static
std::string UpdaterStatusProvider::FetchActiveDirectoryDomain() {
  std::string domain;
  ::DSROLE_PRIMARY_DOMAIN_INFO_BASIC* info = nullptr;
  if (::DsRoleGetPrimaryDomainInformation(nullptr,
                                          ::DsRolePrimaryDomainInfoBasic,
                                          (PBYTE*)&info) != ERROR_SUCCESS) {
    return domain;
  }
  if (info->DomainNameDns)
    domain = base::WideToUTF8(info->DomainNameDns);
  ::DsRoleFreeMemory(info);
  return domain;
}

void UpdaterStatusProvider::OnDomainReceived(std::string domain) {
  domain_ = std::move(domain);
  NotifyStatusChange();
}
