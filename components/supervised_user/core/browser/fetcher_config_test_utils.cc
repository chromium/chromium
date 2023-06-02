// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/fetcher_config_test_utils.h"

#include "base/logging.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace supervised_user {
namespace {

const FetcherConfig& Validated(const FetcherConfig& config) {
  GURL url = GURL(config.service_endpoint).Resolve(config.service_path);
  CHECK(url.is_valid()) << "Invalid service endpoint or path.";
  if (config.histogram_basename.empty()) {
    DLOG(WARNING) << "Histograms are not configured for " << url.spec() << ".";
  }
  return config;
}
}  // namespace

FetcherTestConfigBuilder::FetcherTestConfigBuilder(
    const FetcherConfig& from_config)
    : config_(from_config) {}

FetcherTestConfigBuilder FetcherTestConfigBuilder::FromConfig(
    const FetcherConfig& from_config) {
  return FetcherTestConfigBuilder(from_config);
}

FetcherTestConfigBuilder& FetcherTestConfigBuilder::WithServiceEndpoint(
    base::StringPiece value) {
  config_.service_endpoint = value;
  return *this;
}

FetcherTestConfigBuilder& FetcherTestConfigBuilder::WithServicePath(
    base::StringPiece value) {
  config_.service_path = value;
  return *this;
}

FetcherTestConfigBuilder& FetcherTestConfigBuilder::WithHistogramBasename(
    base::StringPiece value) {
  config_.histogram_basename = value;
  return *this;
}

FetcherConfig FetcherTestConfigBuilder::Build() const {
  return Validated(config_);
}

}  // namespace supervised_user
