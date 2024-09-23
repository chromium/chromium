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
namespace {

constexpr int kTargetArticleCount = 3;

void HandleWebFeedListContentsResponse(
    base::OnceCallback<void(std::vector<NtpFeedContentFetcher::Article>)>
        callback,
    FeedNetwork::ApiResult<feedwire::Response> response) {
  std::vector<NtpFeedContentFetcher::Article> result;
  if (!response.response_body) {
    DLOG(ERROR) << "Failed to fetch content for feed NTP module. Status: "
                << response.response_info.status_code;
    return std::move(callback).Run(std::move(result));
  }

  // Get the first several articles from the response. No attempt is made
  // to show a variety of publishers.
  for (const feedwire::DataOperation& data_operation :
       response.response_body->feed_response().data_operation()) {
    if (data_operation.operation() !=
            feedwire::DataOperation::UPDATE_OR_APPEND ||
        !data_operation.has_feature() ||
        !data_operation.feature().has_content()) {
      continue;
    }

    for (const feedwire::PrefetchMetadata& prefetch_metadata :
         data_operation.feature().content().prefetch_metadata()) {
      auto& article = result.emplace_back();
      article.title = prefetch_metadata.title();
      article.publisher = prefetch_metadata.publisher();
      article.url = GURL(prefetch_metadata.uri());
      article.thumbnail_url = GURL(prefetch_metadata.image_url());
      article.favicon_url = GURL(prefetch_metadata.favicon_url());
      if (result.size() >= kTargetArticleCount)
        return std::move(callback).Run(std::move(result));
    }
  }
  DLOG_IF(ERROR, result.size() < kTargetArticleCount)
      << "Request for content for the feed NTP module succeeded but the "
         "response contained fewer than "
      << kTargetArticleCount << " articles.";
  std::move(callback).Run(std::move(result));
}

}  // namespace

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

void NtpFeedContentFetcher::FetchFollowingFeedArticles(
    base::OnceCallback<void(std::vector<NtpFeedContentFetcher::Article>)>
        callback) {
  feedwire::Request request = CreateFeedQueryRefreshRequest(
      StreamType(StreamKind::kFollowing),
      feedwire::FeedQuery::INTERACTIVE_WEB_FEED, RequestMetadata(),
      /*consistency_token=*/std::string(), SingleWebFeedEntryPoint::kOther,
      /*doc_view_counts=*/{});

  feedwire::ClientInfo* client_info =
      request.mutable_feed_request()->mutable_client_info();
  // TODO(crbug.com/40842320): For the desktop feed prototype we call the
  // Android endpoint and need to pretend we are an Android client. This won't
  // be the case for the launch.
  client_info->set_platform_type(feedwire::ClientInfo::ANDROID_ID);
  client_info->set_app_type(feedwire::ClientInfo::CHROME_ANDROID);

  // For the prototype, it's okay if the request fails or the user is signed
  // out; the module will be empty.
  feed_network_->SendApiRequest<WebFeedListContentsDiscoverApi>(
      request, feed_network_delegate_.GetAccountInfo(), RequestMetadata(),
      base::BindOnce(&HandleWebFeedListContentsResponse, std::move(callback)));
}

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
