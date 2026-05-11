// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_SB_UPDATE_PROTOCOL_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_SB_UPDATE_PROTOCOL_MANAGER_H_

// A class that implements Chrome's interface with the SafeBrowsing V4/V5 update
// protocol.
//
// The SBUpdateProtocolManager handles formatting and making requests of, and
// handling responses from, Google's SafeBrowsing servers. The purpose of this
// class is to get hash prefixes from the SB server for the given set of lists.
// TODO(crbug.com/362791941): Update/extract v4-specific parts of this file.

#include <memory>
#include <optional>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_config.h"
#include "components/safe_browsing/core/common/proto/webui.pb.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {

// Minimum time, in seconds, from start up before we must issue an update query.
static const int kTimerStartIntervalSecMin = 60;

// Maximum time, in seconds, from start up before we must issue an update query.
static const int kTimerStartIntervalSecMax = 300;

// Maximum time, in seconds, to wait for a response to an update request.
static const int kTimerUpdateWaitSecMax = 15 * 60;  // 15 minutes

class SBUpdateProtocolManager {
 public:
  SBUpdateProtocolManager(const SBUpdateProtocolManager&) = delete;
  SBUpdateProtocolManager& operator=(const SBUpdateProtocolManager&) = delete;

  virtual ~SBUpdateProtocolManager();

  // Constructs a SBUpdateProtocolManager that issues network requests using
  // |url_loader_factory|. It schedules updates to get the hash prefixes for
  // SafeBrowsing lists.
  SBUpdateProtocolManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const V4ProtocolConfig& config);

  // Schedule the next update without backoff.
  virtual void ScheduleNextUpdate(
      std::unique_ptr<StoreStateMap> store_state_map) = 0;

  // Populates the UpdateInfo message.
  void CollectUpdateInfo(DatabaseManagerInfo::UpdateInfo* database_info);

  // The time that a response was last received from the server. This will
  // have a null value if no response has been received.
  const base::Time& last_response_time() const;

 protected:
  friend class V5UpdateProtocolManagerTest;
  FRIEND_TEST_ALL_PREFIXES(V4UpdateProtocolManagerTest,
                           TestGetUpdatesErrorHandlingNetwork);
  FRIEND_TEST_ALL_PREFIXES(V4UpdateProtocolManagerTest,
                           TestGetUpdatesErrorHandlingResponseCode);
  FRIEND_TEST_ALL_PREFIXES(V4UpdateProtocolManagerTest, TestGetUpdatesNoError);
  FRIEND_TEST_ALL_PREFIXES(V4UpdateProtocolManagerTest,
                           TestGetUpdatesWithOneBackoff);
  FRIEND_TEST_ALL_PREFIXES(V4UpdateProtocolManagerTest,
                           TestBase64EncodingUsesUrlEncoding);
  FRIEND_TEST_ALL_PREFIXES(V4UpdateProtocolManagerTest, TestDisableAutoUpdates);
  FRIEND_TEST_ALL_PREFIXES(V4UpdateProtocolManagerTest,
                           TestGetUpdatesHasTimeout);
  FRIEND_TEST_ALL_PREFIXES(V4UpdateProtocolManagerTest,
                           TestExtendedReportingLevelIncluded);

  // Records the network response code of the last update
  int last_response_code_ = 0;

  // Resets the update error counter and multiplier.
  void ResetUpdateErrors();

  // Returns whether another update is currently scheduled.
  bool IsUpdateScheduled() const;

  // Get the next update interval, considering whether we are in backoff.
  base::TimeDelta GetNextUpdateInterval(bool back_off);

  // The time delta after which the next update request may be sent.
  // It is set to a random interval between 60 and 300 seconds at start.
  // The server can set it by setting the minimum_wait_duration.
  base::TimeDelta next_update_interval_;

  // The time when the next update is scheduled to be requested. This is valid
  // only when |update_timer_| is running.
  std::optional<base::Time> next_update_time_ = std::nullopt;

  // The config of the client making Pver4 requests.
  const V4ProtocolConfig config_;

  // The URLLoaderFactory we use to issue network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The pending update request. The request must be canceled when the object is
  // destroyed.
  std::unique_ptr<network::SimpleURLLoader> request_;

  // Timer to setup the next update request.
  base::OneShotTimer update_timer_;

  base::Time last_response_time_;

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  // The number of HTTP response errors since the the last successful HTTP
  // response, used for request backoff timing.
  // TODO(crbug.com/362791941): Initialize (feedback from crrev.com/c/7791276).
  size_t update_error_count_;

  // Multiplier for the backoff error after the second.
  // TODO(crbug.com/362791941): Initialize (feedback from crrev.com/c/7791276).
  size_t update_back_off_mult_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_SB_UPDATE_PROTOCOL_MANAGER_H_
