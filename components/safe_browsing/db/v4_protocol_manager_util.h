// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_V4_PROTOCOL_MANAGER_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_DB_V4_PROTOCOL_MANAGER_UTIL_H_

// A class that implements the stateless methods used by the GetHashUpdate and
// GetFullHash stubby calls made by Chrome using the SafeBrowsing V4 protocol.

#include <functional>
#include <initializer_list>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string_piece.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/db/safebrowsing.pb.h"
#include "url/gurl.h"

namespace net {
class HttpRequestHeaders;
class IPAddress;
}  // namespace net

namespace safe_browsing {


// The size of the hash prefix, in bytes. It should be between 4 to 32 (full
// hash).
using PrefixSize = size_t;

// The minimum expected size (in bytes) of a hash-prefix.
const PrefixSize kMinHashPrefixLength = 4;

// The maximum expected size (in bytes) of a hash-prefix. This represents the
// length of a SHA256 hash.
const PrefixSize kMaxHashPrefixLength = 32;

// A hash prefix sent by the SafeBrowsing PVer4 service.
using HashPrefix = std::string;

// A full SHA256 hash.
using FullHash = HashPrefix;

using ListUpdateRequest = FetchThreatListUpdatesRequest::ListUpdateRequest;
using ListUpdateResponse = FetchThreatListUpdatesResponse::ListUpdateResponse;

void SetSbV4UrlPrefixForTesting(const char* url_prefix);

// Config passed to the constructor of a V4 protocol manager.
struct V4ProtocolConfig {
  // The safe browsing client name sent in each request.
  std::string client_name;

  // Disable auto-updates using a command line switch.
  bool disable_auto_update;

  // The Google API key.
  std::string key_param;

  // Current product version sent in each request.
  std::string version;

  V4ProtocolConfig(const std::string& client_name,
                   bool disable_auto_update,
                   const std::string& key_param,
                   const std::string& version);
  V4ProtocolConfig(const V4ProtocolConfig& other);
  ~V4ProtocolConfig();

 private:
  V4ProtocolConfig() = delete;
};

// Get the v4 protocol config struct with a given client name, and ability to
// enable/disable database auto update.
V4ProtocolConfig GetV4ProtocolConfig(const std::string& client_name,
                                     bool disable_auto_update);

// Returns the URL to use for sending threat reports and other Safe Browsing
// hits back to Safe Browsing service.
std::string GetReportUrl(
    const V4ProtocolConfig& config,
    const std::string& method,
    const ExtendedReportingLevel* reporting_level = nullptr);

// Different types of threats that SafeBrowsing protects against. This is the
// type that's returned to the clients of SafeBrowsing in Chromium.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.safe_browsing
// GENERATED_JAVA_PREFIX_TO_STRIP: SB_THREAT_TYPE_
enum SBThreatType {
  // This type can be used for lists that can be checked synchronously so a
  // client callback isn't required, or for whitelists.
  SB_THREAT_TYPE_UNUSED,

  // No threat at all.
  SB_THREAT_TYPE_SAFE,

  // The URL is being used for phishing.
  SB_THREAT_TYPE_URL_PHISHING,

  // The URL hosts malware.
  SB_THREAT_TYPE_URL_MALWARE,

  // The URL hosts unwanted programs.
  SB_THREAT_TYPE_URL_UNWANTED,

  // The download URL is malware.
  SB_THREAT_TYPE_URL_BINARY_MALWARE,

  // Url detected by the client-side phishing model.  Note that unlike the
  // above values, this does not correspond to a downloaded list.
  SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING,

  // The Chrome extension or app (given by its ID) is malware.
  SB_THREAT_TYPE_EXTENSION,

  // Url detected by the client-side malware IP list. This IP list is part
  // of the client side detection model.
  SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE,

  // Url leads to a blacklisted resource script. Note that no warnings should be
  // shown on this threat type, but an incident report might be sent.
  SB_THREAT_TYPE_BLACKLISTED_RESOURCE,

  // Url abuses a permission API.
  SB_THREAT_TYPE_API_ABUSE,

  // Activation patterns for the Subresource Filter.
  SB_THREAT_TYPE_SUBRESOURCE_FILTER,

  // CSD Phishing whitelist.  This "threat" means a URL matched the whitelist.
  SB_THREAT_TYPE_CSD_WHITELIST,

  // DEPRECATED. Url detected by password protection service.
  DEPRECATED_SB_THREAT_TYPE_URL_PASSWORD_PROTECTION_PHISHING,

  // Saved password reuse detected on low reputation page,
  SB_THREAT_TYPE_SAVED_PASSWORD_REUSE,

  // Chrome signed in and syncing gaia password reuse detected on low reputation
  // page,
  SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE,

  // Chrome signed in non syncing gaia password reuse detected on low reputation
  // page,
  SB_THREAT_TYPE_SIGNED_IN_NON_SYNC_PASSWORD_REUSE,

  // A Google ad that caused a blocked autoredirect was collected
  SB_THREAT_TYPE_BLOCKED_AD_REDIRECT,

  // A sample of an ad was collected
  SB_THREAT_TYPE_AD_SAMPLE,

  // A report of Google ad that caused a blocked popup was collected.
  SB_THREAT_TYPE_BLOCKED_AD_POPUP,

  // The page loaded a resource from the Suspicious Site list.
  SB_THREAT_TYPE_SUSPICIOUS_SITE,

  // Enterprise password reuse detected on low reputation page.
  SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE,

  // Potential billing detected.
  SB_THREAT_TYPE_BILLING,

  // Off-market APK file downloaded, which could be potentially dangerous.
  SB_THREAT_TYPE_APK_DOWNLOAD,

  // Match found in the local high-confidence allowlist.
  SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST,
};

using SBThreatTypeSet = base::flat_set<SBThreatType>;

// Return true if |set| only contains types that are valid for CheckBrowseUrl().
// Intended for use in DCHECK().
bool SBThreatTypeSetIsValidForCheckBrowseUrl(const SBThreatTypeSet& set);

// Shorthand for creating an SBThreatTypeSet from a list of SBThreatTypes. Use
// like CreateSBThreatTypeSet({SB_THREAT_TYPE_URL_PHISHING,
//                             SB_THREAT_TYPE_URL_MALWARE})
inline SBThreatTypeSet CreateSBThreatTypeSet(
    std::initializer_list<SBThreatType> set) {
  return SBThreatTypeSet(set);
}

// The information required to uniquely identify each list the client is
// interested in maintaining and downloading from the SafeBrowsing servers.
// For example, for digests of Malware binaries on Windows:
// platform_type = WINDOWS,
// threat_entry_type = EXECUTABLE,
// threat_type = MALWARE
class ListIdentifier {
 public:
  ListIdentifier(PlatformType platform_type,
                 ThreatEntryType threat_entry_type,
                 ThreatType threat_type);
  explicit ListIdentifier(const ListUpdateResponse&);

  bool operator==(const ListIdentifier& other) const;
  bool operator!=(const ListIdentifier& other) const;
  size_t hash() const;

  PlatformType platform_type() const { return platform_type_; }
  ThreatEntryType threat_entry_type() const { return threat_entry_type_; }
  ThreatType threat_type() const { return threat_type_; }

 private:
  PlatformType platform_type_;
  ThreatEntryType threat_entry_type_;
  ThreatType threat_type_;

  ListIdentifier() = delete;
};

std::ostream& operator<<(std::ostream& os, const ListIdentifier& id);

PlatformType GetCurrentPlatformType();
ListIdentifier GetCertCsdDownloadWhitelistId();
ListIdentifier GetChromeExtMalwareId();
ListIdentifier GetChromeUrlApiId();
ListIdentifier GetChromeUrlClientIncidentId();
ListIdentifier GetIpMalwareId();
ListIdentifier GetUrlBillingId();
ListIdentifier GetUrlCsdDownloadWhitelistId();
ListIdentifier GetUrlCsdWhitelistId();
ListIdentifier GetUrlHighConfidenceAllowlistId();
ListIdentifier GetUrlMalBinId();
ListIdentifier GetUrlMalwareId();
ListIdentifier GetUrlSocEngId();
ListIdentifier GetUrlSubresourceFilterId();
ListIdentifier GetUrlSuspiciousSiteId();
ListIdentifier GetUrlUwsId();

// Returns the basename of the store file, without the ".store" extension.
std::string GetUmaSuffixForStore(const base::FilePath& file_path);

// Represents the state of each store.
using StoreStateMap = std::unordered_map<ListIdentifier, std::string>;

// Sever response, parsed in vector form.
using ParsedServerResponse = std::vector<std::unique_ptr<ListUpdateResponse>>;

// Holds the hash prefix and the store that it matched in.
struct StoreAndHashPrefix {
 public:
  ListIdentifier list_id;
  HashPrefix hash_prefix;

  StoreAndHashPrefix(ListIdentifier list_id, const HashPrefix& hash_prefix);
  ~StoreAndHashPrefix();

  bool operator==(const StoreAndHashPrefix& other) const;
  bool operator!=(const StoreAndHashPrefix& other) const;
  size_t hash() const;

 private:
  StoreAndHashPrefix() = delete;
};

// Used to track the hash prefix and the store in which a full hash's prefix
// matched.
using StoreAndHashPrefixes = std::vector<StoreAndHashPrefix>;

// Enumerate failures for histogramming purposes.  DO NOT CHANGE THE
// ORDERING OF THESE VALUES.
enum V4OperationResult {
  // 200 response code means that the server recognized the request.
  STATUS_200 = 0,

  // Subset of successful responses where the response body wasn't parsable.
  PARSE_ERROR = 1,

  // Operation request failed (network error).
  NETWORK_ERROR = 2,

  // Operation request returned HTTP result code other than 200.
  HTTP_ERROR = 3,

  // Operation attempted during error backoff, no request sent.
  BACKOFF_ERROR = 4,

  // Operation attempted before min wait duration elapsed, no request sent.
  MIN_WAIT_DURATION_ERROR = 5,

  // Identical operation already pending.
  ALREADY_PENDING_ERROR = 6,

  // Memory space for histograms is determined by the max.  ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  OPERATION_RESULT_MAX = 7
};

// A class that provides static methods related to the Pver4 protocol.
class V4ProtocolManagerUtil {
 public:
  // Canonicalizes url as per Google Safe Browsing Specification.
  // See: https://developers.google.com/safe-browsing/v4/urls-hashing
  static void CanonicalizeUrl(const GURL& url,
                              std::string* canonicalized_hostname,
                              std::string* canonicalized_path,
                              std::string* canonicalized_query);

  // This method returns the host suffix combinations from the hostname in the
  // URL, as described here:
  // https://developers.google.com/safe-browsing/v4/urls-hashing
  static void GenerateHostVariantsToCheck(const std::string& host,
                                          std::vector<std::string>* hosts);

  // This method returns the path prefix combinations from the path in the
  // URL, as described here:
  // https://developers.google.com/safe-browsing/v4/urls-hashing
  static void GeneratePathVariantsToCheck(const std::string& path,
                                          const std::string& query,
                                          std::vector<std::string>* paths);

  // Given a URL, returns all the patterns we need to check.
  static void GeneratePatternsToCheck(const GURL& url,
                                      std::vector<std::string>* urls);

  // Returns a FullHash for the basic host+path pattern for a given URL after
  // canonicalization. Not intended for general use.
  static FullHash GetFullHash(const GURL& url);

  // Generates a Pver4 request URL and sets the appropriate header values.
  // |request_base64| is the serialized request protocol buffer encoded in
  // base 64.
  // |method_name| is the name of the method to call, as specified in the proto,
  // |config| is an instance of V4ProtocolConfig that stores the client config,
  // |gurl| is set to the value of the PVer4 request URL,
  // |headers| is populated with the appropriate header values.
  static void GetRequestUrlAndHeaders(const std::string& request_base64,
                                      const std::string& method_name,
                                      const V4ProtocolConfig& config,
                                      GURL* gurl,
                                      net::HttpRequestHeaders* headers);

  // Worker function for calculating the backoff times.
  // |multiplier| is doubled for each consecutive error after the
  // first, and |error_count| is incremented with each call.
  // Backoff interval is MIN(((2^(n-1))*15 minutes) * (RAND + 1), 24 hours)
  // where n is the number of consecutive errors.
  static base::TimeDelta GetNextBackOffInterval(size_t* error_count,
                                                size_t* multiplier);

  // Record HTTP response code when there's no error in fetching an HTTP
  // request, and the error code, when there is.
  // |metric_name| is the name of the UMA metric to record the response code or
  // error code against, |net_error| represents the net error code of the HTTP
  // request, and |response code| represents the HTTP response code received
  // from the server.
  static void RecordHttpResponseOrErrorCode(const char* metric_name,
                                            int net_error,
                                            int response_code);

  // Generate the set of FullHashes to check for |url|.
  static void UrlToFullHashes(const GURL& url,
                              std::vector<FullHash>* full_hashes);

  static bool FullHashToHashPrefix(const FullHash& full_hash,
                                   PrefixSize prefix_size,
                                   HashPrefix* hash_prefix);

  static bool FullHashToSmallestHashPrefix(const FullHash& full_hash,
                                           HashPrefix* hash_prefix);

  static bool FullHashMatchesHashPrefix(const FullHash& full_hash,
                                        const HashPrefix& hash_prefix);

  static void SetClientInfoFromConfig(ClientInfo* client_info,
                                      const V4ProtocolConfig& config);

  static bool GetIPV6AddressFromString(const std::string& ip_address,
                                       net::IPAddress* address);

  // Converts a IPV4 or IPV6 address in |ip_address| to the SHA1 hash of the
  // corresponding packed IPV6 address in |hashed_encoded_ip|, and adds an
  // extra byte containing the value 128 at the end. This is done to match the
  // server implementation for calculating the hash prefix of an IP address.
  static bool IPAddressToEncodedIPV6Hash(const std::string& ip_address,
                                         FullHash* hashed_encoded_ip);

  // Stores the client state values for each of the lists in |store_state_map|
  // into |list_client_states|.
  static void GetListClientStatesFromStoreStateMap(
      const std::unique_ptr<StoreStateMap>& store_state_map,
      std::vector<std::string>* list_client_states);

 private:
  V4ProtocolManagerUtil() {}

  FRIEND_TEST_ALL_PREFIXES(V4ProtocolManagerUtilTest, TestBackOffLogic);
  FRIEND_TEST_ALL_PREFIXES(V4ProtocolManagerUtilTest,
                           TestGetRequestUrlAndUpdateHeaders);
  FRIEND_TEST_ALL_PREFIXES(V4ProtocolManagerUtilTest, UrlParsing);
  FRIEND_TEST_ALL_PREFIXES(V4ProtocolManagerUtilTest, CanonicalizeUrl);

  // Composes a URL using |prefix|, |method| (e.g.: encodedFullHashes).
  // |request_base64|, |client_id|, |version| and |key_param|. |prefix|
  // should contain the entire url prefix including scheme, host and path.
  static std::string ComposeUrl(const std::string& prefix,
                                const std::string& method,
                                const std::string& request_base64,
                                const std::string& key_param);

  // Sets the HTTP headers expected by a standard PVer4 request.
  static void UpdateHeaders(net::HttpRequestHeaders* headers);

  // Given a URL, returns all the hosts we need to check.  They are returned
  // in order of size (i.e. b.c is first, then a.b.c).
  static void GenerateHostsToCheck(const GURL& url,
                                   std::vector<std::string>* hosts);

  // Given a URL, returns all the paths we need to check.
  static void GeneratePathsToCheck(const GURL& url,
                                   std::vector<std::string>* paths);

  static std::string RemoveConsecutiveChars(base::StringPiece str,
                                            const char c);

  DISALLOW_COPY_AND_ASSIGN(V4ProtocolManagerUtil);
};

using StoresToCheck = std::unordered_set<ListIdentifier>;

}  // namespace safe_browsing

namespace std {

template <>
struct hash<safe_browsing::PlatformType> {
  std::size_t operator()(const safe_browsing::PlatformType& p) const {
    return std::hash<unsigned int>()(p);
  }
};

template <>
struct hash<safe_browsing::ThreatEntryType> {
  std::size_t operator()(const safe_browsing::ThreatEntryType& tet) const {
    return std::hash<unsigned int>()(tet);
  }
};

template <>
struct hash<safe_browsing::ThreatType> {
  std::size_t operator()(const safe_browsing::ThreatType& tt) const {
    return std::hash<unsigned int>()(tt);
  }
};

template <>
struct hash<safe_browsing::ListIdentifier> {
  std::size_t operator()(const safe_browsing::ListIdentifier& id) const {
    return id.hash();
  }
};

}  // namespace std

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_PROTOCOL_MANAGER_UTIL_H_
