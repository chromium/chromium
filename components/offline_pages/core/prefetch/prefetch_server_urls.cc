// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_server_urls.h"

#include "components/offline_pages/core/offline_page_feature.h"
#include "components/variations/variations_associated_data.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"

namespace offline_pages {

const char kPrefetchServer[] = "https://offlinepages-pa.googleapis.com/";
const char kPrefetchExperimentHeaderName[] = "X-Offline-Prefetch-Experiment";
const char kPrefetchOperationHeaderName[] = "X-Offline-Prefetch-Operation";
const char kPrefetchTestingHeaderName[] = "X-Offline-Prefetch-Testing";

namespace {

const char kOfflinePagesBackend[] = "offline_pages_backend";
const char kGeneratePageBundleRequestURLPath[] = "v1:GeneratePageBundle";
const char kGetOperationLeadingURLPath[] = "v1/";
const char kDownloadLeadingURLPath[] = "v1/media/";

// Used in all offline prefetch request URLs to specify API Key.
const char kApiKeyName[] = "key";
// Needed to download as a file.
const char kAltKeyName[] = "alt";
const char kAltKeyValueForDownload[] = "media";

GURL GetServerURL() {
  GURL endpoint(variations::GetVariationParamValueByFeature(
      offline_pages::kPrefetchingOfflinePagesFeature, kOfflinePagesBackend));

  // |is_valid| returns false for bad URLs and also for empty URLs.
  return endpoint.is_valid() ? endpoint : GURL(kPrefetchServer);
}

GURL GetServerURLForPath(const std::string& url_path) {
  GURL::Replacements replacements;
  replacements.SetPathStr(url_path);
  return GetServerURL().ReplaceComponents(replacements);
}

GURL AppendApiKeyToURL(const GURL& url, version_info::Channel channel) {
  bool is_stable_channel = channel == version_info::Channel::STABLE;
  std::string api_key = is_stable_channel ? google_apis::GetAPIKey()
                                          : google_apis::GetNonStableAPIKey();
  return net::AppendQueryParameter(url, kApiKeyName, api_key);
}

}  // namespace

GURL GeneratePageBundleRequestURL(version_info::Channel channel) {
  GURL server_url = GetServerURLForPath(kGeneratePageBundleRequestURLPath);
  return AppendApiKeyToURL(server_url, channel);
}

GURL GetOperationRequestURL(const std::string& name,
                            version_info::Channel channel) {
  std::string url_path = kGetOperationLeadingURLPath + name;
  GURL server_url = GetServerURLForPath(url_path);
  return AppendApiKeyToURL(server_url, channel);
}

GURL PrefetchDownloadURL(const std::string& download_location,
                         version_info::Channel channel) {
  std::string url_path = kDownloadLeadingURLPath + download_location;
  GURL server_url = GetServerURLForPath(url_path);

  server_url = net::AppendQueryParameter(server_url, kAltKeyName,
                                         kAltKeyValueForDownload);

  return AppendApiKeyToURL(server_url, channel);
}

std::string PrefetchExperimentHeader() {
  std::string tag = GetPrefetchingOfflinePagesExperimentTag();
  if (tag.empty())
    return std::string();
  return std::string(kPrefetchExperimentHeaderName) + ": " + tag;
}

}  // namespace offline_pages
