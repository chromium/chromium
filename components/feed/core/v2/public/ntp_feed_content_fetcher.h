// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_NTP_FEED_CONTENT_FETCHER_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_NTP_FEED_CONTENT_FETCHER_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_network_impl.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "url/gurl.h"

namespace feed {

/** Supports fetching feed content for the desktop NTP module. */
class NtpFeedContentFetcher {
 public:
  NtpFeedContentFetcher(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& api_key,
      PrefService* pref_service);
  virtual ~NtpFeedContentFetcher();

  /** Semantic article info based on PrefetchMetadata. */
  struct Article {
    std::string title;
    std::string publisher;
    GURL url;
    GURL thumbnail_url;
    GURL favicon_url;

    Article();
    Article(const Article&);
    ~Article();
  };

  /**
   * Fetch following feed articles, build a vector of Articles based
   * on their PrefetchMetadata, and pass them along to the callback.
   * @param callback Callback that is called with the fetch result.
   */
  virtual void FetchFollowingFeedArticles(
      base::OnceCallback<void(std::vector<Article>)>);

  void SetFeedNetworkForTesting(std::unique_ptr<FeedNetwork> feed_network);

 private:
  class NetworkDelegate : public FeedNetworkImpl::Delegate {
   public:
    explicit NetworkDelegate(signin::IdentityManager* identity_manager);
    ~NetworkDelegate() override = default;
    std::string GetLanguageTag() override;
    AccountInfo GetAccountInfo() override;
    bool IsOffline() override;

   private:
    // identity_manager_ is unowned and will outlive NtpFeedContentFetcher.
    raw_ptr<signin::IdentityManager> identity_manager_;
  };

  NetworkDelegate feed_network_delegate_;
  std::unique_ptr<FeedNetwork> feed_network_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_NTP_FEED_CONTENT_FETCHER_H_