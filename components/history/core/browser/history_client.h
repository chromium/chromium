// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_CLIENT_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_CLIENT_H_

#include <memory>

#include "base/time/time.h"
#include "sql/init_status.h"

class GURL;

namespace history {

class HistoryBackendClient;
class HistoryService;

// This class abstracts operations that depend on the embedder's environment,
// e.g. Chrome.
class HistoryClient {
 public:
  HistoryClient() {}

  HistoryClient(const HistoryClient&) = delete;
  HistoryClient& operator=(const HistoryClient&) = delete;

  virtual ~HistoryClient() {}

  // Called upon HistoryService creation.
  virtual void OnHistoryServiceCreated(HistoryService* history_service) = 0;

  // Called before HistoryService is shutdown.
  virtual void Shutdown() = 0;

  // Returns true if this look like the type of URL that should be added to the
  // history.
  virtual bool CanAddURL(const GURL& url) = 0;

  // Notifies the embedder that there was a problem reading the database.
  virtual void NotifyProfileError(sql::InitStatus init_status,
                                  const std::string& diagnostics) = 0;

  // Returns a new HistoryBackendClient instance.
  virtual std::unique_ptr<HistoryBackendClient> CreateBackendClient() = 0;

  // Update the last used `time` for the given bookmark node `id`.
  virtual void UpdateBookmarkLastUsedTime(int64_t bookmark_node_id,
                                          base::Time time) = 0;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_CLIENT_H_
