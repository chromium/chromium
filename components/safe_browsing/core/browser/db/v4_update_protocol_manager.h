// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V4_UPDATE_PROTOCOL_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V4_UPDATE_PROTOCOL_MANAGER_H_

#include "components/safe_browsing/core/browser/db/sb_update_protocol_manager.h"

class GURL;

namespace safe_browsing {

// V4UpdateCallback is invoked when a scheduled update completes.
// Parameters:
//   - The vector of update response protobufs received from the server for
//     each list type.
using V4UpdateCallback =
    base::RepeatingCallback<void(std::unique_ptr<ParsedServerResponse>)>;

using ExtendedReportingLevelCallback =
    base::RepeatingCallback<ExtendedReportingLevel()>;

class V4UpdateProtocolManager : public SBUpdateProtocolManager {
 public:
  V4UpdateProtocolManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const V4ProtocolConfig& config,
      V4UpdateCallback update_callback,
      ExtendedReportingLevelCallback extended_reporting_level_callback);
  ~V4UpdateProtocolManager() override;

  // Schedule the next update without backoff.
  void ScheduleNextUpdate(
      std::unique_ptr<StoreStateMap> store_state_map) override;

 protected:
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

  void OnURLLoaderComplete(std::optional<std::string> response_body);
  void OnURLLoaderCompleteInternal(int net_error,
                                   int response_code,
                                   const std::string& data);

  // Fills a FetchThreatListUpdatesRequest protocol buffer for a request.
  // Returns the serialized and base64 URL encoded request as a string.
  std::string GetBase64SerializedUpdateRequestProto();

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

  // Called when update request times out. Cancels the existing request and
  // re-sends the update request.
  void HandleTimeout();

  // Updates internal update and backoff state for each update response error,
  // assuming that the current time is |now|.
  void HandleUpdateError(const base::Time& now);

  // Generates the URL for the update request and issues the request for the
  // lists passed to the constructor.
  void IssueUpdateRequest();

  // Schedule the next update with backoff specified.
  void ScheduleNextUpdateWithBackoff(bool back_off);

  // Schedule the next update, after the given interval.
  void ScheduleNextUpdateAfterInterval(base::TimeDelta interval);

 private:
  // The last known state of the lists.
  // Updated after every successful update of the database.
  std::unique_ptr<StoreStateMap> store_state_map_;

  // The callback that's called when GetUpdates completes.
  V4UpdateCallback update_callback_;

  // Used to interrupt and re-schedule update requests that take too long to
  // complete.
  base::OneShotTimer timeout_timer_;

  ExtendedReportingLevelCallback extended_reporting_level_callback_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V4_UPDATE_PROTOCOL_MANAGER_H_
