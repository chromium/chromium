// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/ntp_feed_content_fetcher.h"

#include <memory>
#include "components/feed/core/proto/v2/wire/client_info.pb.h"
#include "components/feed/core/proto/v2/wire/stream_structure.pb.h"
#include "components/feed/core/v2/feed_network_impl.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/feed_feature_list.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace feed {
NtpFeedContentFetcher::NtpFeedContentFetcher(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& api_key,
    PrefService* pref_service)
    : feed_network_delegate_(identity_manager) {
  feed_network_ = std::make_unique<FeedNetworkImpl>(
      &feed_network_delegate_, identity_manager, api_key, url_loader_factory,
      pref_service);
}

NtpFeedContentFetcher::~NtpFeedContentFetcher() = default;

NtpFeedContentFetcher::Article::Article() = default;
NtpFeedContentFetcher::Article::Article(const Article&) = default;
NtpFeedContentFetcher::Article::~Article() = default;

void NtpFeedContentFetcher::SetFeedNetworkForTesting(
    std::unique_ptr<FeedNetwork> feed_network) {
  feed_network_ = std::move(feed_network);
}

NtpFeedContentFetcher::NetworkDelegate::NetworkDelegate(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {}

std::string NtpFeedContentFetcher::NetworkDelegate::GetLanguageTag() {
  // TODO(crbug.com/40842320): Change this for the final implementation.
  return std::string();
}

AccountInfo NtpFeedContentFetcher::NetworkDelegate::GetAccountInfo() {
  return AccountInfo(identity_manager_->GetPrimaryAccountInfo(
      GetConsentLevelNeededForPersonalizedFeed()));
}

bool NtpFeedContentFetcher::NetworkDelegate::IsOffline() {
  // TODO(crbug.com/40842320): Change this for the final implementation.
  return false;
}

}  // namespace feed
