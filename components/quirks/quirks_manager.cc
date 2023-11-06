// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/quirks/quirks_manager.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/quirks/pref_names.h"
#include "components/quirks/quirks_client.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace quirks {

namespace {

QuirksManager* manager_ = nullptr;

const char kIccExtension[] = ".icc";

// How often we query Quirks Server.
const int kDaysBetweenServerChecks = 30;

// Check if QuirksClient has already downloaded icc file from server.
base::FilePath CheckForIccFile(const base::FilePath& path) {
  const bool exists = base::PathExists(path);
  VLOG(1) << (exists ? "File" : "No File") << " found at " << path.value();
  // TODO(glevin): If file exists, do we want to implement a hash to verify that
  // the file hasn't been corrupted or tampered with?
  return exists ? path : base::FilePath();
}

}  // namespace

std::string IdToHexString(int64_t product_id) {
  return base::StringPrintf("%08" PRIx64, product_id);
}

std::string IdToFileName(int64_t product_id) {
  return IdToHexString(product_id).append(kIccExtension);
}

////////////////////////////////////////////////////////////////////////////////
// QuirksManager

QuirksManager::QuirksManager(
    std::unique_ptr<Delegate> delegate,
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : waiting_for_login_(true),
      delegate_(std::move(delegate)),
      task_runner_(base::ThreadPool::CreateTaskRunner({base::MayBlock()})),
      local_state_(local_state),
      url_loader_factory_(std::move(url_loader_factory)) {}

QuirksManager::~QuirksManager() {
  clients_.clear();
  manager_ = nullptr;
}

// static
void QuirksManager::Initialize(
    std::unique_ptr<Delegate> delegate,
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  manager_ = new QuirksManager(std::move(delegate), local_state,
                               std::move(url_loader_factory));
}

// static
void QuirksManager::Shutdown() {
  delete manager_;
}

// static
QuirksManager* QuirksManager::Get() {
  DCHECK(manager_);
  return manager_;
}

// static
bool QuirksManager::HasInstance() {
  return !!manager_;
}

// static
void QuirksManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kQuirksClientLastServerCheck);
}

// Delay downloads until after login, to ensure that device policy has been set.
void QuirksManager::OnLoginCompleted() {
  if (!waiting_for_login_)
    return;

  waiting_for_login_ = false;
  if (!clients_.empty() && !QuirksEnabled()) {
    VLOG(2) << clients_.size() << " client(s) deleted.";
    clients_.clear();
  }

  for (const std::unique_ptr<QuirksClient>& client : clients_)
    client->StartDownload();
}

void QuirksManager::RequestIccProfilePath(
    int64_t product_id,
    const std::string& display_name,
    RequestFinishedCallback on_request_finished) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!QuirksEnabled()) {
    VLOG(1) << "Quirks Client disabled.";
    std::move(on_request_finished).Run(base::FilePath(), false);
    return;
  }

  if (!product_id) {
    VLOG(1) << "Could not determine display information (product id = 0)";
    std::move(on_request_finished).Run(base::FilePath(), false);
    return;
  }

  std::string name = IdToFileName(product_id);
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CheckForIccFile,
                     delegate_->GetDisplayProfileDirectory().Append(name)),
      base::BindOnce(&QuirksManager::OnIccFilePathRequestCompleted,
                     weak_ptr_factory_.GetWeakPtr(), product_id, display_name,
                     std::move(on_request_finished)));
}

void QuirksManager::ClientFinished(QuirksClient* client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SetLastServerCheck(client->product_id(), base::Time::Now());
  auto it = clients_.find(client);
  CHECK(it != clients_.end());
  clients_.erase(it);
}

void QuirksManager::OnIccFilePathRequestCompleted(
    int64_t product_id,
    const std::string& display_name,
    RequestFinishedCallback on_request_finished,
    base::FilePath path) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If we found a file, just inform requester.
  if (!path.empty()) {
    std::move(on_request_finished).Run(path, false);
    // TODO(glevin): If Quirks files are ever modified on the server, we'll need
    // to modify this logic to check for updates. See crbug.com/595024.
    return;
  }

  double last_check = local_state_->GetDict(prefs::kQuirksClientLastServerCheck)
                          .FindDouble(IdToHexString(product_id))
                          .value_or(0.0);

  const base::TimeDelta time_since =
      base::Time::Now() - base::Time::FromSecondsSinceUnixEpoch(last_check);

  // Don't need server check if we've checked within last 30 days.
  if (time_since < base::Days(kDaysBetweenServerChecks)) {
    VLOG(2) << time_since.InDays()
            << " days since last Quirks Server check for display "
            << IdToHexString(product_id);
    std::move(on_request_finished).Run(base::FilePath(), false);
    return;
  }

  // Create and start a client to download file.
  QuirksClient* client = new QuirksClient(product_id, display_name,
                                          std::move(on_request_finished), this);
  clients_.insert(base::WrapUnique(client));
  if (!waiting_for_login_)
    client->StartDownload();
  else
    VLOG(2) << "Quirks Client created; waiting for login to begin download.";
}

bool QuirksManager::QuirksEnabled() {
  if (!delegate_->DevicePolicyEnabled()) {
    VLOG(2) << "Quirks Client disabled by device policy.";
    return false;
  }
  return true;
}

void QuirksManager::SetLastServerCheck(int64_t product_id,
                                       const base::Time& last_check) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScopedDictPrefUpdate dict(local_state_, prefs::kQuirksClientLastServerCheck);
  dict->Set(IdToHexString(product_id), last_check.InSecondsFSinceUnixEpoch());
}

}  // namespace quirks
