// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_UPDATE_PROTOCOL_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_UPDATE_PROTOCOL_MANAGER_H_

// A class that implements Chrome's interface with the SafeBrowsing V5 update
// protocol.
//
// The V5UpdateProtocolManager sends requests to Google Safe Browsing servers
// for the V5 BatchGetHashLists API. This API is responsible for fetching lists
// of full hashes or hash prefixes, or updates to those lists.
// TODO(crbug.com/362791941): remove v4 references

#include <memory>
#include <optional>
#include <string>

#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/sb_update_protocol_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_config.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

class GURL;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {

// V5UpdateCallback is invoked every time a scheduled update completes.
// Parameters:
//   - The mapping of update response protobufs received from the server for
//     each list type. This can be `std::nullopt` if the update failed.
using V5UpdateCallback = base::RepeatingCallback<void(
    std::optional<std::map<ListIdentifier, V5::HashList>>)>;

class V5UpdateProtocolManager : public SBUpdateProtocolManager {
 public:
  V5UpdateProtocolManager(const V5UpdateProtocolManager&) = delete;
  V5UpdateProtocolManager& operator=(const V5UpdateProtocolManager&) = delete;

  ~V5UpdateProtocolManager() override;

  // Constructs a V5UpdateProtocolManager that issues network requests using
  // `url_loader_factory`. It schedules updates to get the SafeBrowsing lists
  // and invokes `update_callback` when the results are retrieved.
  V5UpdateProtocolManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const V4ProtocolConfig& config,
      V5UpdateCallback update_callback);

  struct ListIdentifierAndVersion {
    ListIdentifierAndVersion(ListIdentifier list_identifier,
                             std::string list_version);
    // Identifies a specific Safe Browsing list.
    ListIdentifier list_identifier;
    // Refers to the hash list version sent by the server for the particular
    // list.
    std::string list_version;
  };

  // Schedule the next update without accounting for backoff. The
  // `store_state_map` contains the mapping of each list to its hash list
  // version.
  void ScheduleNextUpdate(
      std::unique_ptr<StoreStateMap> store_state_map) override;

 private:
  friend class V5UpdateProtocolManagerTest;

  // The parsed response from the server.
  // TODO(crbug.com/362791941): add minimum_wait_duration
  struct ParsedResponse {
    ParsedResponse();
    explicit ParsedResponse(
        std::map<ListIdentifier, V5::HashList> hash_list_map);
    ~ParsedResponse();
    ParsedResponse(ParsedResponse&&);
    ParsedResponse& operator=(ParsedResponse&&);

    // The list updates.
    std::map<ListIdentifier, V5::HashList> hash_list_map;
  };

  // Called when the request completes. Parses the response and calls
  // `update_callback_` or schedules the next update attempt upon failure.
  void OnURLLoaderComplete(
      std::vector<ListIdentifierAndVersion> list_identifier_to_version_mapping,
      std::optional<std::string> response_body);

  // Helper method for `OnURLLoaderComplete` that returns the result of fetching
  // and parsing the response.
  ParsedResponse OnURLLoaderCompleteInternal(
      int net_error,
      int response_code,
      const std::optional<std::string>& response_body,
      const std::vector<ListIdentifierAndVersion>&
          list_identifier_to_version_mapping);

  // Fills a `BatchGetHashListsRequest` protocol buffer for a request.
  // Returns the serialized and base64 URL encoded request as a string.
  static std::string GetBase64SerializedUpdateRequestProto(
      const std::vector<ListIdentifierAndVersion>&
          list_identifier_to_version_mapping);

  // Parses the base64 encoded response received from the server as a
  // `BatchGetHashListsResponse` protobuf.
  // Returns the mapping per requested `ListIdentifier` to the `HashList`
  // returned from the server.
  ParsedResponse ParseUpdateResponse(
      const std::optional<std::string>& response_body,
      const std::vector<ListIdentifierAndVersion>&
          list_identifier_to_version_mapping);

  // Generates the URL for the update request and issues the request for the
  // requested lists.
  void IssueUpdateRequest(
      std::vector<ListIdentifierAndVersion> list_identifier_to_version_mapping);

  // Returns whether another update is currently scheduled.
  bool IsUpdateScheduled() const;

  // Schedule the next update, specifying whether to back off.
  void ScheduleNextUpdateInternal(
      bool back_off,
      std::vector<ListIdentifierAndVersion> list_identifier_to_version_mapping);

  // Schedule the next update after the given interval.
  void ScheduleNextUpdateAfterInterval(
      base::TimeDelta interval,
      std::vector<ListIdentifierAndVersion> list_identifier_to_version_mapping);

  // The callback that's called when fetching lists completes.
  V5UpdateCallback update_callback_;

  base::WeakPtrFactory<V5UpdateProtocolManager> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_UPDATE_PROTOCOL_MANAGER_H_
