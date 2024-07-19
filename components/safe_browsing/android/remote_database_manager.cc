// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/android/remote_database_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/timer/elapsed_timer.h"
#include "components/safe_browsing/android/real_time_url_checks_allowlist.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"
#include "components/safe_browsing/core/browser/db/v4_get_hash_protocol_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using content::BrowserThread;
namespace safe_browsing {

using IsInAllowlistResult = RealTimeUrlChecksAllowlist::IsInAllowlistResult;
namespace {

constexpr char kCanCheckUrlBaseHistogramName[] = "SB2.RemoteCall.CanCheckUrl";

void LogCanCheckUrl(bool can_check_url, CheckBrowseUrlType check_type) {
  base::UmaHistogramBoolean(kCanCheckUrlBaseHistogramName, can_check_url);
  std::string metrics_suffix;
  switch (check_type) {
    case CheckBrowseUrlType::kHashDatabase:
      metrics_suffix = ".HashDatabase";
      break;
    case CheckBrowseUrlType::kHashRealTime:
      metrics_suffix = ".HashRealTime";
      break;
  }
  base::UmaHistogramBoolean(kCanCheckUrlBaseHistogramName + metrics_suffix,
                            can_check_url);
}

}  // namespace

//
// RemoteSafeBrowsingDatabaseManager::ClientRequest methods
//
class RemoteSafeBrowsingDatabaseManager::ClientRequest {
 public:
  enum class CallbackType {
    BROWSE_URL,
    DOWNLOAD_URL,
  };

  ClientRequest(Client* client,
                CallbackType callback_type,
                RemoteSafeBrowsingDatabaseManager* db_manager,
                const std::vector<GURL>& urls);
  void OnRequestDone(SBThreatType matched_threat_type,
                     const ThreatMetadata& metadata);
  void AddPendingCheck();

  // Accessors
  Client* client() const { return client_; }
  const std::vector<GURL>& urls() const { return urls_; }
  size_t pending_checks() const { return pending_checks_; }
  base::WeakPtr<ClientRequest> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void CompleteCheck();

  raw_ptr<Client, DanglingUntriaged> client_;
  CallbackType callback_type_;
  raw_ptr<RemoteSafeBrowsingDatabaseManager, DanglingUntriaged> db_manager_;
  std::vector<GURL> urls_;
  size_t pending_checks_ = 0;
  SBThreatType most_severe_threat_type_ = SBThreatType::SB_THREAT_TYPE_SAFE;
  ThreatMetadata most_severe_metadata_;
  base::ElapsedTimer timer_;
  base::WeakPtrFactory<ClientRequest> weak_factory_{this};
};

RemoteSafeBrowsingDatabaseManager::ClientRequest::ClientRequest(
    Client* client,
    CallbackType callback_type,
    RemoteSafeBrowsingDatabaseManager* db_manager,
    const std::vector<GURL>& urls)
    : client_(client),
      callback_type_(callback_type),
      db_manager_(db_manager),
      urls_(urls) {}

void RemoteSafeBrowsingDatabaseManager::ClientRequest::OnRequestDone(
    SBThreatType matched_threat_type,
    const ThreatMetadata& metadata) {
  DVLOG(1) << "OnRequestDone took " << timer_.Elapsed().InMilliseconds()
           << " ms for client " << client_;

  if (most_severe_threat_type_ == SBThreatType::SB_THREAT_TYPE_SAFE) {
    most_severe_threat_type_ = matched_threat_type;
    most_severe_metadata_ = metadata;
  }

  --pending_checks_;

  if (pending_checks_ == 0) {
    CompleteCheck();
  }
}

void RemoteSafeBrowsingDatabaseManager::ClientRequest::AddPendingCheck() {
  ++pending_checks_;
}

void RemoteSafeBrowsingDatabaseManager::ClientRequest::CompleteCheck() {
  switch (callback_type_) {
    case CallbackType::BROWSE_URL:
      client_->OnCheckBrowseUrlResult(urls_[0], most_severe_threat_type_,
                                      most_severe_metadata_);
      break;
    case CallbackType::DOWNLOAD_URL:
      client_->OnCheckDownloadUrlResult(urls_, most_severe_threat_type_);
      break;
  }
  UMA_HISTOGRAM_TIMES("SB2.RemoteCall.Elapsed", timer_.Elapsed());
  // CancelCheck() will delete *this.
  db_manager_->CancelCheck(client_);
}

//
// RemoteSafeBrowsingDatabaseManager methods
//

RemoteSafeBrowsingDatabaseManager::RemoteSafeBrowsingDatabaseManager()
    : SafeBrowsingDatabaseManager(content::GetUIThreadTaskRunner({}),
                                  content::GetIOThreadTaskRunner({})),
      enabled_(false) {}

RemoteSafeBrowsingDatabaseManager::~RemoteSafeBrowsingDatabaseManager() {
  DCHECK(!enabled_);
}

void RemoteSafeBrowsingDatabaseManager::CancelCheck(Client* client) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(enabled_);
  for (auto itr = current_requests_.begin(); itr != current_requests_.end();
       ++itr) {
    if ((*itr)->client() == client) {
      current_requests_.erase(itr);
      return;
    }
  }
}

bool RemoteSafeBrowsingDatabaseManager::CanCheckUrl(const GURL& url) const {
  return url.SchemeIsHTTPOrHTTPS() || url.SchemeIs(url::kFtpScheme) ||
         url.SchemeIsWSOrWSS();
}

bool RemoteSafeBrowsingDatabaseManager::CheckBrowseUrl(
    const GURL& url,
    const SBThreatTypeSet& threat_types,
    Client* client,
    CheckBrowseUrlType check_type) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(!threat_types.empty());
  DCHECK(SBThreatTypeSetIsValidForCheckBrowseUrl(threat_types));
  if (!enabled_) {
    return true;
  }

  bool can_check_url = CanCheckUrl(url);
  LogCanCheckUrl(can_check_url, check_type);
  if (!can_check_url) {
    return true;  // Safe, continue right away.
  }

  auto req = std::make_unique<ClientRequest>(
      client, ClientRequest::CallbackType::BROWSE_URL, this,
      std::vector<GURL>{url});

  DVLOG(1) << "Checking for client " << client << " and URL " << url;
  auto callback =
      std::make_unique<SafeBrowsingApiHandlerBridge::ResponseCallback>(
          base::BindOnce(&ClientRequest::OnRequestDone, req->GetWeakPtr()));
  req->AddPendingCheck();
  switch (check_type) {
    case CheckBrowseUrlType::kHashDatabase:
      SafeBrowsingApiHandlerBridge::GetInstance().StartHashDatabaseUrlCheck(
          std::move(callback), url, threat_types);
      break;
    case CheckBrowseUrlType::kHashRealTime:
      SafeBrowsingApiHandlerBridge::GetInstance().StartHashRealTimeUrlCheck(
          std::move(callback), url, threat_types);
  }
  current_requests_.push_back(std::move(req));

  // Defer the resource load.
  return false;
}

bool RemoteSafeBrowsingDatabaseManager::CheckDownloadUrl(
    const std::vector<GURL>& url_chain,
    Client* client) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());

  if (!enabled_) {
    return true;
  }

  auto req = std::make_unique<ClientRequest>(
      client, ClientRequest::CallbackType::DOWNLOAD_URL, this, url_chain);

  // Must add pending checks in a separate loop so that synchronous responses
  // from the SafeBrowsingApiHandlerBridge can't complete the check early.
  for (const GURL& url : url_chain) {
    if (!CanCheckUrl(url)) {
      continue;
    }
    req->AddPendingCheck();
  }

  if (req->pending_checks() == 0) {
    return true;
  }

  for (const GURL& url : url_chain) {
    if (!CanCheckUrl(url)) {
      continue;
    }
    DVLOG(1) << "Checking for client " << client << " and URL " << url;
    auto callback =
        std::make_unique<SafeBrowsingApiHandlerBridge::ResponseCallback>(
            base::BindOnce(&ClientRequest::OnRequestDone, req->GetWeakPtr()));
    SafeBrowsingApiHandlerBridge::GetInstance().StartHashDatabaseUrlCheck(
        std::move(callback), url,
        CreateSBThreatTypeSet({SBThreatType::SB_THREAT_TYPE_URL_MALWARE,
                               SBThreatType::SB_THREAT_TYPE_URL_UNWANTED}));
  }

  current_requests_.push_back(std::move(req));

  // Defer the resource load.
  return false;
}

bool RemoteSafeBrowsingDatabaseManager::CheckExtensionIDs(
    const std::set<std::string>& extension_ids,
    Client* client) {
  NOTREACHED_IN_MIGRATION();
  return true;
}

bool RemoteSafeBrowsingDatabaseManager::CheckResourceUrl(const GURL& url,
                                                         Client* client) {
  NOTREACHED_IN_MIGRATION();
  return true;
}

std::optional<
    SafeBrowsingDatabaseManager::HighConfidenceAllowlistCheckLoggingDetails>
RemoteSafeBrowsingDatabaseManager::CheckUrlForHighConfidenceAllowlist(
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());

  if (!enabled_ || !CanCheckUrl(url)) {
    sb_task_runner()->PostTask(FROM_HERE,
                               base::BindOnce(std::move(callback), false));
    return std::nullopt;
  }

  IsInAllowlistResult match_result =
      RealTimeUrlChecksAllowlist::GetInstance()->IsInAllowlist(url);
  // Note that if the allowlist is unavailable, we say that is a match.
  bool is_match = match_result == IsInAllowlistResult::kInAllowlist ||
                  match_result == IsInAllowlistResult::kAllowlistUnavailable;
  sb_task_runner()->PostTask(FROM_HERE,
                             base::BindOnce(std::move(callback), is_match));
  return std::nullopt;
}

bool RemoteSafeBrowsingDatabaseManager::CheckUrlForSubresourceFilter(
    const GURL& url,
    Client* client) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());

  if (!enabled_ || !CanCheckUrl(url)) {
    return true;
  }

  auto req = std::make_unique<ClientRequest>(
      client, ClientRequest::CallbackType::BROWSE_URL, this,
      std::vector<GURL>{url});

  DVLOG(1) << "Checking for client " << client << " and URL " << url;
  auto callback =
      std::make_unique<SafeBrowsingApiHandlerBridge::ResponseCallback>(
          base::BindOnce(&ClientRequest::OnRequestDone, req->GetWeakPtr()));
  req->AddPendingCheck();
  SafeBrowsingApiHandlerBridge::GetInstance().StartHashDatabaseUrlCheck(
      std::move(callback), url,
      CreateSBThreatTypeSet({SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER,
                             SBThreatType::SB_THREAT_TYPE_URL_PHISHING}));

  current_requests_.push_back(std::move(req));

  // Defer the resource load.
  return false;
}

AsyncMatch RemoteSafeBrowsingDatabaseManager::CheckCsdAllowlistUrl(
    const GURL& url,
    Client* client) {
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());

  // If this URL's scheme isn't supported, call is safe.
  if (!CanCheckUrl(url)) {
    return AsyncMatch::MATCH;
  }

  // TODO(crbug.com/41477281): Make this call async.
  bool is_match =
      SafeBrowsingApiHandlerBridge::GetInstance().StartCSDAllowlistCheck(url);
  return is_match ? AsyncMatch::MATCH : AsyncMatch::NO_MATCH;
}

void RemoteSafeBrowsingDatabaseManager::MatchDownloadAllowlistUrl(
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  NOTREACHED_IN_MIGRATION();
  sb_task_runner()->PostTask(FROM_HERE,
                             base::BindOnce(std::move(callback), true));
}

safe_browsing::ThreatSource
RemoteSafeBrowsingDatabaseManager::GetBrowseUrlThreatSource(
    CheckBrowseUrlType check_type) const {
  switch (check_type) {
    case CheckBrowseUrlType::kHashDatabase:
      return base::FeatureList::IsEnabled(
                 kSafeBrowsingNewGmsApiForBrowseUrlDatabaseCheck)
                 ? safe_browsing::ThreatSource::ANDROID_SAFEBROWSING
                 : safe_browsing::ThreatSource::REMOTE;
    case CheckBrowseUrlType::kHashRealTime:
      return safe_browsing::ThreatSource::ANDROID_SAFEBROWSING_REAL_TIME;
  }
}

safe_browsing::ThreatSource
RemoteSafeBrowsingDatabaseManager::GetNonBrowseUrlThreatSource() const {
  return safe_browsing::ThreatSource::REMOTE;
}

void RemoteSafeBrowsingDatabaseManager::StartOnSBThread(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const V4ProtocolConfig& config) {
  VLOG(1) << "RemoteSafeBrowsingDatabaseManager starting";
  SafeBrowsingDatabaseManager::StartOnSBThread(url_loader_factory, config);

  enabled_ = true;
}

void RemoteSafeBrowsingDatabaseManager::StopOnSBThread(bool shutdown) {
  // |shutdown| is not used.
  DCHECK(sb_task_runner()->RunsTasksInCurrentSequence());
  DVLOG(1) << "RemoteSafeBrowsingDatabaseManager stopping";

  // Call back and delete any remaining clients. OnRequestDone() modifies
  // |current_requests_|, so we make a copy first.
  std::vector<std::unique_ptr<ClientRequest>> to_callback(
      std::move(current_requests_));
  for (const std::unique_ptr<ClientRequest>& req : to_callback) {
    req->OnRequestDone(SBThreatType::SB_THREAT_TYPE_SAFE, ThreatMetadata());
  }
  enabled_ = false;

  SafeBrowsingDatabaseManager::StopOnSBThread(shutdown);
}

bool RemoteSafeBrowsingDatabaseManager::IsDatabaseReady() const {
  return enabled_;
}

}  // namespace safe_browsing
