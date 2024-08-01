// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"

#include <string_view>

#include "base/base64.h"
#include "base/hash/hash.h"
#include "base/hash/sha1.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"
#include "crypto/sha2.h"
#include "google_apis/google_api_keys.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "url/url_util.h"

using base::Time;

namespace safe_browsing {

// Can be overriden by tests.
const char* g_sbv4_url_prefix_for_testing = nullptr;

const char kSbV4UrlPrefix[] = "https://safebrowsing.googleapis.com/v4";

const base::FilePath::CharType kStoreSuffix[] = FILE_PATH_LITERAL(".store");

namespace {

// The default URL prefix where browser reports safe browsing hits and malware
// details.
const char kSbReportsURLPrefix[] =
    "https://safebrowsing.google.com/safebrowsing";

std::string Unescape(const std::string& url) {
  std::string unescaped_str(url);
  const int kMaxLoopIterations = 1024;
  size_t old_size = 0;
  int loop_var = 0;
  do {
    old_size = unescaped_str.size();
    unescaped_str = base::UnescapeBinaryURLComponent(unescaped_str);
  } while (old_size != unescaped_str.size() &&
           ++loop_var <= kMaxLoopIterations);

  return unescaped_str;
}

std::string Escape(const std::string& url) {
  std::string escaped_str;
  // The escaped string is larger so allocate double the length to reduce the
  // chance of the string being grown.
  escaped_str.reserve(url.length() * 2);
  for (size_t i = 0; i < url.length(); i++) {
    unsigned char c = static_cast<unsigned char>(url[i]);
    if (c <= ' ' || c > '~' || c == '#' || c == '%') {
      escaped_str += '%';
      base::AppendHexEncodedByte(c, escaped_str);
    } else {
      escaped_str += c;
    }
  }

  return escaped_str;
}

}  // namespace

V4ProtocolConfig GetV4ProtocolConfig(const std::string& client_name,
                                     bool disable_auto_update) {
  return V4ProtocolConfig(client_name, disable_auto_update,
                          google_apis::GetAPIKey(),
                          std::string(version_info::GetVersionNumber()));
}

void SetSbV4UrlPrefixForTesting(const char* url_prefix) {
  g_sbv4_url_prefix_for_testing = url_prefix;
}

std::string GetReportUrl(const V4ProtocolConfig& config,
                         const std::string& method,
                         const ExtendedReportingLevel* reporting_level,
                         const bool is_enhanced_protection) {
  std::string url = base::StringPrintf(
      "%s/%s?client=%s&appver=%s&pver=4.0", kSbReportsURLPrefix, method.c_str(),
      config.client_name.c_str(), config.version.c_str());
  std::string api_key = google_apis::GetAPIKey();
  if (!api_key.empty()) {
    base::StringAppendF(&url, "&key=%s",
                        base::EscapeQueryParamValue(api_key, true).c_str());
  }
  if (reporting_level)
    url.append(
        base::StringPrintf("&ext=%d", static_cast<int>(*reporting_level)));
  if (is_enhanced_protection)
    url.append(base::StringPrintf("&enh=%d", is_enhanced_protection));
  return url;
}

std::ostream& operator<<(std::ostream& os, const ListIdentifier& id) {
  os << "{hash: " << id.hash() << "; platform_type: " << id.platform_type()
     << "; threat_entry_type: " << id.threat_entry_type()
     << "; threat_type: " << id.threat_type() << "}";
  return os;
}

PlatformType GetCurrentPlatformType() {
#if BUILDFLAG(IS_WIN)
  return WINDOWS_PLATFORM;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return LINUX_PLATFORM;
#elif BUILDFLAG(IS_IOS)
  return IOS_PLATFORM;
#elif BUILDFLAG(IS_MAC)
  return OSX_PLATFORM;
#else
  return ANDROID_PLATFORM;
#endif
}

ListIdentifier GetChromeExtMalwareId() {
  return ListIdentifier(CHROME_PLATFORM, CHROME_EXTENSION, MALWARE_THREAT);
}

ListIdentifier GetChromeUrlApiId() {
  return ListIdentifier(GetCurrentPlatformType(), URL, API_ABUSE);
}

ListIdentifier GetUrlBillingId() {
  return ListIdentifier(GetCurrentPlatformType(), URL, BILLING);
}

ListIdentifier GetUrlCsdDownloadAllowlistId() {
  return ListIdentifier(GetCurrentPlatformType(), URL, CSD_DOWNLOAD_ALLOWLIST);
}

ListIdentifier GetUrlCsdAllowlistId() {
  return ListIdentifier(GetCurrentPlatformType(), URL, CSD_ALLOWLIST);
}

ListIdentifier GetUrlHighConfidenceAllowlistId() {
  return ListIdentifier(GetCurrentPlatformType(), URL,
                        HIGH_CONFIDENCE_ALLOWLIST);
}

ListIdentifier GetUrlMalwareId() {
  return ListIdentifier(GetCurrentPlatformType(), URL, MALWARE_THREAT);
}

ListIdentifier GetUrlMalBinId() {
  return ListIdentifier(GetCurrentPlatformType(), URL, MALICIOUS_BINARY);
}

ListIdentifier GetUrlSocEngId() {
  return ListIdentifier(GetCurrentPlatformType(), URL, SOCIAL_ENGINEERING);
}

ListIdentifier GetUrlSubresourceFilterId() {
  return ListIdentifier(GetCurrentPlatformType(), URL, SUBRESOURCE_FILTER);
}

ListIdentifier GetUrlSuspiciousSiteId() {
  return ListIdentifier(GetCurrentPlatformType(), URL, SUSPICIOUS);
}

ListIdentifier GetUrlUwsId() {
  return ListIdentifier(GetCurrentPlatformType(), URL, UNWANTED_SOFTWARE);
}

std::string GetUmaSuffixForStore(const base::FilePath& file_path) {
  DCHECK_EQ(kStoreSuffix, file_path.BaseName().Extension());
  return base::StringPrintf(
      ".%" PRFilePath, file_path.BaseName().RemoveExtension().value().c_str());
}

StoreAndHashPrefix::StoreAndHashPrefix(ListIdentifier list_id,
                                       const HashPrefixStr& hash_prefix)
    : list_id(list_id), hash_prefix(hash_prefix) {}

StoreAndHashPrefix::~StoreAndHashPrefix() {}

bool StoreAndHashPrefix::operator==(const StoreAndHashPrefix& other) const {
  return list_id == other.list_id && hash_prefix == other.hash_prefix;
}

bool StoreAndHashPrefix::operator!=(const StoreAndHashPrefix& other) const {
  return !operator==(other);
}

size_t StoreAndHashPrefix::hash() const {
  std::size_t first = list_id.hash();
  std::size_t second = std::hash<std::string>()(hash_prefix);

  return base::HashInts(first, second);
}

bool SBThreatTypeSetIsValidForCheckBrowseUrl(const SBThreatTypeSet& set) {
  for (SBThreatType type : set) {
    switch (type) {
      case SBThreatType::SB_THREAT_TYPE_URL_PHISHING:
      case SBThreatType::SB_THREAT_TYPE_URL_MALWARE:
      case SBThreatType::SB_THREAT_TYPE_URL_UNWANTED:
      case SBThreatType::SB_THREAT_TYPE_SUSPICIOUS_SITE:
      case SBThreatType::SB_THREAT_TYPE_BILLING:
        break;

      default:
        return false;
    }
  }
  return true;
}

bool ListIdentifier::operator==(const ListIdentifier& other) const {
  return platform_type_ == other.platform_type_ &&
         threat_entry_type_ == other.threat_entry_type_ &&
         threat_type_ == other.threat_type_;
}

bool ListIdentifier::operator!=(const ListIdentifier& other) const {
  return !operator==(other);
}

size_t ListIdentifier::hash() const {
  std::size_t first = std::hash<unsigned int>()(platform_type_);
  std::size_t second = std::hash<unsigned int>()(threat_entry_type_);
  std::size_t third = std::hash<unsigned int>()(threat_type_);

  std::size_t interim = base::HashInts(first, second);
  return base::HashInts(interim, third);
}

ListIdentifier::ListIdentifier(PlatformType platform_type,
                               ThreatEntryType threat_entry_type,
                               ThreatType threat_type)
    : platform_type_(platform_type),
      threat_entry_type_(threat_entry_type),
      threat_type_(threat_type) {
  DCHECK(PlatformType_IsValid(platform_type));
  DCHECK(ThreatEntryType_IsValid(threat_entry_type));
  DCHECK(ThreatType_IsValid(threat_type));
}

ListIdentifier::ListIdentifier(const ListUpdateResponse& response)
    : ListIdentifier(response.platform_type(),
                     response.threat_entry_type(),
                     response.threat_type()) {}

V4ProtocolConfig::V4ProtocolConfig(const std::string& client_name,
                                   bool disable_auto_update,
                                   const std::string& key_param,
                                   const std::string& version)
    : client_name(client_name),
      disable_auto_update(disable_auto_update),
      key_param(key_param),
      version(version) {}

V4ProtocolConfig::V4ProtocolConfig(const V4ProtocolConfig& other) = default;

V4ProtocolConfig::~V4ProtocolConfig() {}

// static
base::TimeDelta V4ProtocolManagerUtil::GetNextBackOffInterval(
    size_t* error_count,
    size_t* multiplier) {
  DCHECK(multiplier && error_count);
  (*error_count)++;
  if (*error_count > 1 && *error_count < 9) {
    // With error count 9 and above we will hit the 24 hour max interval.
    // Cap the multiplier here to prevent integer overflow errors.
    *multiplier *= 2;
  }
  base::TimeDelta next =
      base::Minutes(*multiplier * (1 + base::RandDouble()) * 15);
  base::TimeDelta day = base::Hours(24);
  return next < day ? next : day;
}

// static
void V4ProtocolManagerUtil::GetRequestUrlAndHeaders(
    const std::string& request_base64,
    const std::string& method_name,
    const V4ProtocolConfig& config,
    GURL* gurl,
    net::HttpRequestHeaders* headers) {
  const char* url_prefix = g_sbv4_url_prefix_for_testing
                               ? g_sbv4_url_prefix_for_testing
                               : kSbV4UrlPrefix;
  *gurl = GURL(
      ComposeUrl(url_prefix, method_name, request_base64, config.key_param));
  UpdateHeaders(headers);
}

// static
std::string V4ProtocolManagerUtil::ComposeUrl(const std::string& prefix,
                                              const std::string& method,
                                              const std::string& request_base64,
                                              const std::string& key_param) {
  DCHECK(!prefix.empty() && !method.empty());
  std::string url = base::StringPrintf(
      "%s/%s?$req=%s&$ct=application/x-protobuf", prefix.c_str(),
      method.c_str(), request_base64.c_str());
  if (!key_param.empty()) {
    base::StringAppendF(&url, "&key=%s",
                        base::EscapeQueryParamValue(key_param, true).c_str());
  }
  return url;
}

// static
void V4ProtocolManagerUtil::UpdateHeaders(net::HttpRequestHeaders* headers) {
  // NOTE(vakh): The following header informs the envelope server (which sits in
  // front of Google's stubby server) that the received GET request should be
  // interpreted as a POST.
  headers->SetHeaderIfMissing("X-HTTP-Method-Override", "POST");
}

// static
void V4ProtocolManagerUtil::UrlToFullHashes(
    const GURL& url,
    std::vector<FullHashStr>* full_hashes) {
  std::string canon_host, canon_path, canon_query;
  CanonicalizeUrl(url, &canon_host, &canon_path, &canon_query);

  std::vector<std::string> hosts;
  if (url.HostIsIPAddress()) {
    hosts.push_back(url.host());
  } else {
    GenerateHostVariantsToCheck(canon_host, &hosts);
  }

  std::vector<std::string> paths;
  GeneratePathVariantsToCheck(canon_path, canon_query, &paths);

  full_hashes->reserve(full_hashes->size() + hosts.size() * paths.size());
  for (const std::string& host : hosts) {
    for (const std::string& path : paths) {
      full_hashes->push_back(crypto::SHA256HashString(host + path));
    }
  }
}

// static
bool V4ProtocolManagerUtil::FullHashToHashPrefix(const FullHashStr& full_hash,
                                                 PrefixSize prefix_size,
                                                 HashPrefixStr* hash_prefix) {
  if (full_hash.size() < prefix_size) {
    return false;
  }
  *hash_prefix = full_hash.substr(0, prefix_size);
  return true;
}

// static
bool V4ProtocolManagerUtil::FullHashToSmallestHashPrefix(
    const FullHashStr& full_hash,
    HashPrefixStr* hash_prefix) {
  return FullHashToHashPrefix(full_hash, kMinHashPrefixLength, hash_prefix);
}

// static
bool V4ProtocolManagerUtil::FullHashMatchesHashPrefix(
    const FullHashStr& full_hash,
    const HashPrefixStr& hash_prefix) {
  return full_hash.compare(0, hash_prefix.length(), hash_prefix) == 0;
}

// static
void V4ProtocolManagerUtil::GenerateHostsToCheck(
    const GURL& url,
    std::vector<std::string>* hosts) {
  std::string canon_host;
  CanonicalizeUrl(url, &canon_host, nullptr, nullptr);
  GenerateHostVariantsToCheck(canon_host, hosts);
}

// static
void V4ProtocolManagerUtil::GeneratePathsToCheck(
    const GURL& url,
    std::vector<std::string>* paths) {
  std::string canon_path;
  std::string canon_query;
  CanonicalizeUrl(url, nullptr, &canon_path, &canon_query);
  GeneratePathVariantsToCheck(canon_path, canon_query, paths);
}

// static
void V4ProtocolManagerUtil::GeneratePatternsToCheck(
    const GURL& url,
    std::vector<std::string>* urls) {
  std::string canon_host;
  std::string canon_path;
  std::string canon_query;
  CanonicalizeUrl(url, &canon_host, &canon_path, &canon_query);

  std::vector<std::string> hosts, paths;
  GenerateHostVariantsToCheck(canon_host, &hosts);
  GeneratePathVariantsToCheck(canon_path, canon_query, &paths);
  for (size_t h = 0; h < hosts.size(); ++h) {
    for (size_t p = 0; p < paths.size(); ++p) {
      urls->push_back(hosts[h] + paths[p]);
    }
  }
}

// static
FullHashStr V4ProtocolManagerUtil::GetFullHash(const GURL& url) {
  std::string host;
  std::string path;
  CanonicalizeUrl(url, &host, &path, nullptr);

  return crypto::SHA256HashString(host + path);
}

// static
void V4ProtocolManagerUtil::CanonicalizeUrl(const GURL& url,
                                            std::string* canonicalized_hostname,
                                            std::string* canonicalized_path,
                                            std::string* canonicalized_query) {
  DCHECK(url.is_valid());

  // We only canonicalize "normal" URLs.
  if (!url.IsStandard())
    return;

  // Following canonicalization steps are excluded since url parsing takes care
  // of those :-
  // 1. Remove any tab (0x09), CR (0x0d), and LF (0x0a) chars from url.
  //    (Exclude escaped version of these chars).
  // 2. Normalize hostname to 4 dot-seperated decimal values.
  // 3. Lowercase hostname.
  // 4. Resolve path sequences "/../" and "/./".

  // That leaves us with the following :-
  // 1. Remove fragment in URL.
  GURL url_without_fragment;
  GURL::Replacements f_replacements;
  f_replacements.ClearRef();
  f_replacements.ClearUsername();
  f_replacements.ClearPassword();
  url_without_fragment = url.ReplaceComponents(f_replacements);

  // 2. Do URL unescaping until no more hex encoded characters exist.
  std::string url_unescaped_str(Unescape(url_without_fragment.spec()));
  std::string_view url_unescaped_str_view(url_unescaped_str);
  url::Parsed parsed = url::ParseStandardURL(url_unescaped_str);

  // 3. In hostname, remove all leading and trailing dots.
  std::string_view host;
  if (parsed.host.is_nonempty())
    host = url_unescaped_str_view.substr(parsed.host.begin, parsed.host.len);

  std::string_view host_without_end_dots =
      base::TrimString(host, ".", base::TrimPositions::TRIM_ALL);

  // 4. In hostname, replace consecutive dots with a single dot.
  std::string host_without_consecutive_dots(
      RemoveConsecutiveChars(host_without_end_dots, '.'));

  // 5. In path, replace runs of consecutive slashes with a single slash.
  std::string_view path;
  if (parsed.path.is_nonempty())
    path = url_unescaped_str_view.substr(parsed.path.begin, parsed.path.len);
  std::string path_without_consecutive_slash(RemoveConsecutiveChars(path, '/'));

  url::Replacements<char> hp_replacements;
  hp_replacements.SetHost(
      host_without_consecutive_dots.data(),
      url::Component(0, host_without_consecutive_dots.length()));
  hp_replacements.SetPath(
      path_without_consecutive_slash.data(),
      url::Component(0, path_without_consecutive_slash.length()));

  std::string url_unescaped_with_can_hostpath;
  url::StdStringCanonOutput output(&url_unescaped_with_can_hostpath);
  url::Parsed temp_parsed;
  url::ReplaceComponents(url_unescaped_str.data(), url_unescaped_str.length(),
                         parsed, hp_replacements, nullptr, &output,
                         &temp_parsed);
  output.Complete();

  // 6. Step needed to revert escaping done in url::ReplaceComponents.
  url_unescaped_with_can_hostpath = Unescape(url_unescaped_with_can_hostpath);

  // 7. After performing all above steps, percent-escape all chars in url which
  // are <= ASCII 32, >= 127, #, %. Escapes must be uppercase hex characters.
  std::string escaped_canon_url_str(Escape(url_unescaped_with_can_hostpath));
  url::Parsed final_parsed = url::ParseStandardURL(escaped_canon_url_str);

  if (canonicalized_hostname && final_parsed.host.is_nonempty()) {
    *canonicalized_hostname = escaped_canon_url_str.substr(
        final_parsed.host.begin, final_parsed.host.len);
  }
  if (canonicalized_path && final_parsed.path.is_nonempty()) {
    *canonicalized_path = escaped_canon_url_str.substr(final_parsed.path.begin,
                                                       final_parsed.path.len);
  }
  if (canonicalized_query && final_parsed.query.is_nonempty()) {
    *canonicalized_query = escaped_canon_url_str.substr(
        final_parsed.query.begin, final_parsed.query.len);
  }
}

// static
std::string V4ProtocolManagerUtil::RemoveConsecutiveChars(std::string_view str,
                                                          const char c) {
  std::string output;
  // Output is at most the length of the original string.
  output.reserve(str.size());

  size_t i = 0;
  while (i < str.size()) {
    output.append(1, str[i++]);
    if (str[i - 1] == c) {
      while (i < str.size() && str[i] == c) {
        i++;
      }
    }
  }

  return output;
}

// static
void V4ProtocolManagerUtil::GenerateHostVariantsToCheck(
    const std::string& host,
    std::vector<std::string>* hosts) {
  hosts->clear();

  if (host.empty())
    return;

  // Per the Safe Browsing Protocol v2 spec, we try the host, and also up to 4
  // hostnames formed by starting with the last 5 components and successively
  // removing the leading component.  The last component isn't examined alone,
  // since it's the TLD or a subcomponent thereof.
  //
  // Note that we don't need to be clever about stopping at the "real" eTLD --
  // the data on the server side has been filtered to ensure it will not
  // blocklist a whole TLD, and it's not significantly slower on our side to
  // just check too much.
  //
  // Also note that because we have a simple blocklist, not some sort of complex
  // allowlist-in-blocklist or vice versa, it doesn't matter what order we check
  // these in.
  const size_t kMaxHostsToCheck = 4;
  bool skipped_last_component = false;
  for (std::string::const_reverse_iterator i(host.rbegin());
       i != host.rend() && hosts->size() < kMaxHostsToCheck; ++i) {
    if (*i == '.') {
      if (skipped_last_component)
        hosts->push_back(std::string(i.base(), host.end()));
      else
        skipped_last_component = true;
    }
  }
  hosts->push_back(host);
}

// static
void V4ProtocolManagerUtil::GeneratePathVariantsToCheck(
    const std::string& path,
    const std::string& query,
    std::vector<std::string>* paths) {
  paths->clear();

  if (path.empty())
    return;

  // Per the Safe Browsing Protocol v2 spec, we try the exact path with/without
  // the query parameters, and also up to 4 paths formed by starting at the root
  // and adding more path components.
  //
  // As with the hosts above, it doesn't matter what order we check these in.
  const size_t kMaxPathsToCheck = 4;
  for (std::string::const_iterator i(path.begin());
       i != path.end() && paths->size() < kMaxPathsToCheck; ++i) {
    if (*i == '/')
      paths->push_back(std::string(path.begin(), i + 1));
  }

  if (!paths->empty() && paths->back() != path)
    paths->push_back(path);

  if (!query.empty())
    paths->push_back(path + "?" + query);
}

// static
void V4ProtocolManagerUtil::SetClientInfoFromConfig(
    ClientInfo* client_info,
    const V4ProtocolConfig& config) {
  DCHECK(client_info);
  client_info->set_client_id(config.client_name);
  client_info->set_client_version(config.version);
}

// static
void V4ProtocolManagerUtil::GetListClientStatesFromStoreStateMap(
    const std::unique_ptr<StoreStateMap>& store_state_map,
    std::vector<std::string>* list_client_states) {
  base::ranges::transform(*store_state_map,
                          std::back_inserter(*list_client_states),
                          &StoreStateMap::value_type::second);
}

}  // namespace safe_browsing
