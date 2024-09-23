// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/safe_browsing_request.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/timer/timer.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace {

// The maximum amount of time to wait for the Safe Browsing response.
constexpr base::TimeDelta kSafeBrowsingCheckTimeout = base::Seconds(2);

}  // namespace

// SafeBrowsingRequest::SafeBrowsingClient --------------------------

class SafeBrowsingRequest::SafeBrowsingClient
    : public safe_browsing::SafeBrowsingDatabaseManager::Client {
 public:
  SafeBrowsingClient(scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
                         database_manager,
                     base::WeakPtr<SafeBrowsingRequest> handler,
                     scoped_refptr<base::TaskRunner> handler_task_runner)
      : database_manager_(database_manager),
        handler_(handler),
        handler_task_runner_(handler_task_runner) {}

  ~SafeBrowsingClient() override {
    if (timeout_.IsRunning())
      database_manager_->CancelApiCheck(this);
  }

  void CheckUrl(const GURL& url) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // Start the timer before the call to CheckDownloadUrl(), as it may
    // call back into CheckDownloadUrl() synchronously.
    timeout_.Start(FROM_HERE, kSafeBrowsingCheckTimeout, this,
                   &SafeBrowsingClient::OnTimeout);

    if (database_manager_->CheckDownloadUrl({url}, this)) {
      timeout_.AbandonAndStop();
      SendResultToHandler(/*is_url_safe=*/true);
    }
  }

 private:
  SafeBrowsingClient(const SafeBrowsingClient&) = delete;
  SafeBrowsingClient& operator=(const SafeBrowsingClient&) = delete;

  void OnTimeout() {
    database_manager_->CancelApiCheck(this);
    SendResultToHandler(/*is_url_safe=*/true);
  }

  void SendResultToHandler(bool is_url_safe) {
    handler_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SafeBrowsingRequest::OnResultReceived,
                                  handler_, is_url_safe));
  }

  // SafeBrowsingDatabaseManager::Client:
  void OnCheckDownloadUrlResult(
      const std::vector<GURL>& url_chain,
      safe_browsing::SBThreatType threat_type) override {
    timeout_.AbandonAndStop();
    bool is_url_safe =
        threat_type == safe_browsing::SBThreatType::SB_THREAT_TYPE_SAFE;
    SendResultToHandler(is_url_safe);
  }

  base::OneShotTimer timeout_;
  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager_;
  base::WeakPtr<SafeBrowsingRequest> handler_;
  scoped_refptr<base::TaskRunner> handler_task_runner_;
};

// SafeBrowsingRequest ----------------------------------------------

SafeBrowsingRequest::SafeBrowsingRequest(
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    const GURL& url,
    base::OnceCallback<void(bool)> callback)
    : callback_(std::move(callback)) {
  client_ = std::make_unique<SafeBrowsingClient>(
      database_manager, weak_factory_.GetWeakPtr(),
      base::SequencedTaskRunner::GetCurrentDefault());
  client_->CheckUrl(url);
}

SafeBrowsingRequest::~SafeBrowsingRequest() = default;

void SafeBrowsingRequest::OnResultReceived(bool is_url_safe) {
  DCHECK(callback_);
  std::move(callback_).Run(is_url_safe);
}
