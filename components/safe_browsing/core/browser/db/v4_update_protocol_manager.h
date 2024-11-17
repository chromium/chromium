// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V4_UPDATE_PROTOCOL_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V4_UPDATE_PROTOCOL_MANAGER_H_

// A class that implements Chrome's interface with the SafeBrowsing V4 update
// protocol.
//
// The V4UpdateProtocolManager handles formatting and making requests of, and
// handling responses from, Google's SafeBrowsing servers. The purpose of this
// class is to get hash prefixes from the SB server for the given set of lists.

#include <memory>
#include <optional>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/safe_browsing/core/browser/db/safebrowsing.pb.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/common/proto/webui.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

class GURL;

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {

// V4UpdateCallback is invoked when a scheduled update completes.
// Parameters:
//   - The vector of update response protobufs received from the server for
//     each list type.
using V4UpdateCallback =
    base::RepeatingCallback<void(std::unique_ptr<ParsedServerResponse>)>;

using ExtendedReportingLevelCallback =
    base::RepeatingCallback<ExtendedReportingLevel()>;

class V4UpdateProtocolManager {
 public:
  V4UpdateProtocolManager(const V4UpdateProtocolManager&) = delete;
  V4UpdateProtocolManager& operator=(const V4UpdateProtocolManager&) = delete;

  ~V4UpdateProtocolManager();

  // Constructs a V4UpdateProtocolManager that issues network requests using
  // |url_loader_factory|. It schedules updates to get the hash prefixes for
  // SafeBrowsing lists, and invoke |callback| when the results are retrieved.
  // The callback may be invoked synchronously.
  V4UpdateProtocolManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const V4ProtocolConfig& config,
      V4UpdateCallback update_callback,
      ExtendedReportingLevelCallback extended_reporting_level_callback);

  void OnURLLoaderComplete(std::unique_ptr<std::string> response_body);

  // Schedule the next update without backoff.
  void ScheduleNextUpdate(std::unique_ptr<StoreStateMap> store_state_map);

  // Populates the UpdateInfo message.
  void CollectUpdateInfo(DatabaseManagerInfo::UpdateInfo* database_info);

  // The time that a response was last received from the server. This will
  // have a null value if no response has been received.
  const base::Time& last_response_time() const;

 private:
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

  void OnURLLoaderCompleteInternal(int net_error,
                                   int response_code,
                                   const std::string& data);

  // Fills a FetchThreatListUpdatesRequest protocol buffer for a request.
  // Returns the serialized and base64 URL encoded request as a string.
  std::string GetBase64SerializedUpdateRequestProto();

  // Records the network response code of the last update
  int last_response_code_ = 0;

  // The method to populate |gurl| with the URL to be sent to the server.
  // |request_base64| is the base64 encoded form of an instance of the protobuf
  // FetchThreatListUpdatesRequest. Also sets the appropriate header values for
  // sending PVer4 requests in |headers|.
  void GetUpdateUrlAndHeaders(const std::string& request_base64,
                              GURL* gurl,
                              net::HttpRequestHeaders* headers) const;

  // Parses the base64 encoded response received from the server as a
  // FetchThreatListUpdatesResponse protobuf and returns each of the
  // ListUpdateResponse protobufs contained in it as a vector.
  // Returns true if parsing is successful, false otherwise.
  bool ParseUpdateResponse(const std::string& data_base64,
                           ParsedServerResponse* parsed_server_response);

  // Resets the update error counter and multiplier.
  void ResetUpdateErrors();

  // Called when update request times out. Cancels the existing request and
  // re-sends the update request.
  void HandleTimeout();

  // Updates internal update and backoff state for each update response error,
  // assuming that the current time is |now|.
  void HandleUpdateError(const base::Time& now);

  // Generates the URL for the update request and issues the request for the
  // lists passed to the constructor.
  void IssueUpdateRequest();

  // Returns whether another update is currently scheduled.
  bool IsUpdateScheduled() const;

  // Schedule the next update with backoff specified.
  void ScheduleNextUpdateWithBackoff(bool back_off);

  // Schedule the next update, after the given interval.
  void ScheduleNextUpdateAfterInterval(base::TimeDelta interval);

  // Get the next update interval, considering whether we are in backoff.
  base::TimeDelta GetNextUpdateInterval(bool back_off);

  // The last known state of the lists.
  // Updated after every successful update of the database.
  std::unique_ptr<StoreStateMap> store_state_map_;

  // The number of HTTP response errors since the the last successful HTTP
  // response, used for request backoff timing.
  size_t update_error_count_;

  // Multiplier for the backoff error after the second.
  size_t update_back_off_mult_;

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

  // The callback that's called when GetUpdates completes.
  V4UpdateCallback update_callback_;

  // The pending update request. The request must be canceled when the object is
  // destroyed.
  std::unique_ptr<network::SimpleURLLoader> request_;

  // Timer to setup the next update request.
  base::OneShotTimer update_timer_;

  base::Time last_response_time_;

  // Used to interrupt and re-schedule update requests that take too long to
  // complete.
  base::OneShotTimer timeout_timer_;

  ExtendedReportingLevelCallback extended_reporting_level_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V4_UPDATE_PROTOCOL_MANAGER_H_
