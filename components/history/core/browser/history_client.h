// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_CLIENT_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_CLIENT_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "sql/init_status.h"

class GURL;

namespace history {

class HistoryBackendClient;
class HistoryService;

using CanAddURLCallback = base::RepeatingCallback<bool(const GURL&)>;

// This class abstracts operations that depend on the embedder's environment,
// e.g. Chrome.
class HistoryClient {
 public:
  HistoryClient() = default;

  HistoryClient(const HistoryClient&) = delete;
  HistoryClient& operator=(const HistoryClient&) = delete;

  virtual ~HistoryClient() = default;

  // Called upon HistoryService creation.
  virtual void OnHistoryServiceCreated(HistoryService* history_service) = 0;

  // Called before HistoryService is shutdown.
  virtual void Shutdown() = 0;

  // Returns a callback that determined whether the given URL should be added to
  // history.
  // NOTE: The callback must be safe to call from any thread! (This method
  // should still only be called from the UI thread though.)
  virtual CanAddURLCallback GetThreadSafeCanAddURLCallback() const = 0;

  // Notifies the embedder that there was a problem reading the database.
  virtual void NotifyProfileError(sql::InitStatus init_status,
                                  const std::string& diagnostics) = 0;

  // Returns a new HistoryBackendClient instance.
  virtual std::unique_ptr<HistoryBackendClient> CreateBackendClient() = 0;

  // Update the last used `time` for the given `bookmark_node_id`.
  virtual void UpdateBookmarkLastUsedTime(int64_t bookmark_node_id,
                                          base::Time time) = 0;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_CLIENT_H_
