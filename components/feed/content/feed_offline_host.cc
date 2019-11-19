// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/content/feed_offline_host.h"

#include <utility>

#include "base/bind.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/feed/core/feed_scheduler_host.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/page_criteria.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "url/gurl.h"

namespace feed {

using offline_pages::OfflinePageItem;
using offline_pages::OfflinePageModel;
using offline_pages::PrefetchService;
using offline_pages::PrefetchSuggestion;
using offline_pages::SuggestionsProvider;

namespace {

// Aggregates multiple callbacks from OfflinePageModel, storing the offline url.
// When all callbacks have been invoked, tracked by ref counting, then
// |on_completeion_| is finally invoked, sending all results together.
class CallbackAggregator : public base::RefCounted<CallbackAggregator> {
 public:
  using ReportStatusCallback =
      base::OnceCallback<void(std::vector<std::string>)>;
  using CacheIdCallback =
      base::RepeatingCallback<void(const std::string&, int64_t)>;

  CallbackAggregator(OfflinePageModel* model,
                     ReportStatusCallback on_completion,
                     CacheIdCallback on_each_result)
      : on_completion_(std::move(on_completion)),
        on_each_result_(std::move(on_each_result)),
        start_time_(base::Time::Now()) {}

  // We curry |feed_url|, which is the URL the Feed requested with. This is used
  // instead of the URLs present in the |pages| because offline pages has
  // non-exact URL matching, and we must communicate with the Feed with exact
  // matches.
  void OnGetPages(std::string feed_url,
                  const std::vector<OfflinePageItem>& pages) {
    if (!pages.empty()) {
      OfflinePageItem best =
          *std::max_element(pages.begin(), pages.end(), [](auto lhs, auto rhs) {
            // Prefer prefetched articles over any other. They are typically of
            // higher quality.
            bool leftIsPrefetch = lhs.client_id.name_space ==
                                  offline_pages::kSuggestedArticlesNamespace;
            bool rightIsPrefetch = rhs.client_id.name_space ==
                                   offline_pages::kSuggestedArticlesNamespace;
            if (leftIsPrefetch != rightIsPrefetch) {
              // Only one is prefetch, if that is |rhs|, then they're in the
              // correct order.
              return rightIsPrefetch;
            } else {
              // Newer articles are also better, but not as important as being
              // prefetched.
              return lhs.creation_time < rhs.creation_time;
            }
          });
      urls_.push_back(feed_url);
      on_each_result_.Run(feed_url, best.offline_id);
    }
  }

 private:
  friend class base::RefCounted<CallbackAggregator>;

  ~CallbackAggregator() {
    base::TimeDelta duration = base::Time::Now() - start_time_;
    UMA_HISTOGRAM_TIMES("ContentSuggestions.Feed.Offline.GetStatusDuration",
                        duration);
    std::move(on_completion_).Run(std::move(urls_));
  }

  OfflinePageModel* offline_page_model_;

  // To be called once all callbacks are run or destroyed.
  ReportStatusCallback on_completion_;

  // The urls of the offlined pages seen so far. Ultimately will be given to
  // |on_completeion_|.
  std::vector<std::string> urls_;

  // Is not run if there are no results for a given url.
  CacheIdCallback on_each_result_;

  // The time the aggregator was created, before any requests were sent to the
  // OfflinePageModel.
  base::Time start_time_;
};

// Consumes |metadataVector|, moving as many of the fields as possible.
std::vector<PrefetchSuggestion> ConvertMetadataToSuggestions(
    std::vector<ContentMetadata> metadataVector) {
  std::vector<PrefetchSuggestion> suggestionsVector;
  for (ContentMetadata& metadata : metadataVector) {
    // TODO(skym): Copy over published time when PrefetchSuggestion adds
    // support.
    PrefetchSuggestion suggestion;
    suggestion.article_url = GURL(metadata.url);
    suggestion.article_title = std::move(metadata.title);
    suggestion.article_attribution = std::move(metadata.publisher);
    suggestion.article_snippet = std::move(metadata.snippet);
    suggestion.thumbnail_url = GURL(metadata.image_url);
    suggestion.favicon_url = GURL(metadata.favicon_url);
    suggestionsVector.push_back(std::move(suggestion));
  }
  return suggestionsVector;
}

void RunSuggestionCallbackWithConversion(
    SuggestionsProvider::SuggestionCallback suggestions_callback,
    std::vector<offline_pages::PrefetchSuggestion> metadataVector) {
  std::move(suggestions_callback).Run(metadataVector);
}

}  //  namespace

FeedOfflineHost::FeedOfflineHost(OfflinePageModel* offline_page_model,
                                 PrefetchService* prefetch_service,
                                 base::RepeatingClosure on_suggestion_consumed,
                                 base::RepeatingClosure on_suggestions_shown)
    : offline_page_model_(offline_page_model),
      prefetch_service_(prefetch_service),
      on_suggestion_consumed_(on_suggestion_consumed),
      on_suggestions_shown_(on_suggestions_shown) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(offline_page_model_);
  DCHECK(prefetch_service_);
  DCHECK(!on_suggestion_consumed_.is_null());
  DCHECK(!on_suggestions_shown_.is_null());
}

FeedOfflineHost::~FeedOfflineHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Safe to call RemoveObserver() even if AddObserver() has not been called.
  offline_page_model_->RemoveObserver(this);
}

void FeedOfflineHost::Initialize(
    const base::RepeatingClosure& trigger_get_known_content,
    const NotifyStatusChangeCallback& notify_status_change) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(trigger_get_known_content_.is_null());
  DCHECK(!trigger_get_known_content.is_null());
  DCHECK(notify_status_change_.is_null());
  DCHECK(!notify_status_change.is_null());
  trigger_get_known_content_ = trigger_get_known_content;
  notify_status_change_ = notify_status_change;
  offline_page_model_->AddObserver(this);
  // The host guarantees that the two callbacks passed into this method will not
  // be invoked until Initialize() has exited. To guarantee this, the host
  // cannot call SetSuggestionProvider() in task, because that would give
  // Prefetch the ability to run |trigger_get_known_content_| immediately.
  // PostTask is used to delay when SetSuggestionProvider() is called.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FeedOfflineHost::SetSuggestionProvider,
                                weak_factory_.GetWeakPtr()));
}

void FeedOfflineHost::SetSuggestionProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  prefetch_service_->SetSuggestionProvider(this);
}

base::Optional<int64_t> FeedOfflineHost::GetOfflineId(const std::string& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iter = url_hash_to_id_.find(base::FastHash(url));
  return iter == url_hash_to_id_.end() ? base::Optional<int64_t>()
                                       : base::Optional<int64_t>(iter->second);
}

void FeedOfflineHost::GetOfflineStatus(
    std::vector<std::string> urls,
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UMA_HISTOGRAM_EXACT_LINEAR("ContentSuggestions.Feed.Offline.GetStatusCount",
                             urls.size(), 50);

  scoped_refptr<CallbackAggregator> aggregator =
      base::MakeRefCounted<CallbackAggregator>(
          offline_page_model_, std::move(callback),
          base::BindRepeating(&FeedOfflineHost::CacheOfflinePageUrlAndId,
                              weak_factory_.GetWeakPtr()));

  for (std::string url : urls) {
    offline_pages::PageCriteria criteria;
    criteria.url = GURL(url);
    criteria.exclude_tab_bound_pages = true;
    offline_page_model_->GetPagesWithCriteria(
        criteria, base::BindOnce(&CallbackAggregator::OnGetPages, aggregator,
                                 std::move(url)));
  }
}

void FeedOfflineHost::OnContentRemoved(std::vector<std::string> urls) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const std::string& url : urls) {
    prefetch_service_->RemoveSuggestion(GURL(url));
  }
}

void FeedOfflineHost::OnNewContentReceived() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  prefetch_service_->NewSuggestionsAvailable();
}

void FeedOfflineHost::OnNoListeners() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url_hash_to_id_.clear();
}

void FeedOfflineHost::OnGetKnownContentDone(
    std::vector<ContentMetadata> suggestions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // While |suggestions| are movable, there might be multiple callbacks in
  // |pending_known_content_callbacks_|. To be safe, copy all the suggestions.
  std::vector<offline_pages::PrefetchSuggestion> converted_suggestions =
      ConvertMetadataToSuggestions(std::move(suggestions));
  for (auto& callback : pending_known_content_callbacks_) {
    RunSuggestionCallbackWithConversion(std::move(callback),
                                        converted_suggestions);
  }
  pending_known_content_callbacks_.clear();
}

void FeedOfflineHost::GetCurrentArticleSuggestions(
    SuggestionsProvider::SuggestionCallback suggestions_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!trigger_get_known_content_.is_null());
  pending_known_content_callbacks_.emplace_back(
      std::move(suggestions_callback));
  // Trigger after push_back() in case triggering results in a synchronous
  // response via OnGetKnownContentDone().
  if (pending_known_content_callbacks_.size() <= 1) {
    trigger_get_known_content_.Run();
  }
}

void FeedOfflineHost::ReportArticleListViewed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_suggestion_consumed_.Run();
}

void FeedOfflineHost::ReportArticleViewed(GURL article_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_suggestions_shown_.Run();
}

void FeedOfflineHost::OfflinePageModelLoaded(OfflinePageModel* model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ignored.
}

void FeedOfflineHost::OfflinePageAdded(OfflinePageModel* model,
                                       const OfflinePageItem& added_page) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!notify_status_change_.is_null());
  const std::string& url = added_page.GetOriginalUrl().spec();
  CacheOfflinePageUrlAndId(url, added_page.offline_id);
  notify_status_change_.Run(url, true);
}

void FeedOfflineHost::OfflinePageDeleted(const OfflinePageItem& deleted_page) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!notify_status_change_.is_null());
  const std::string& url = deleted_page.url.spec();
  EvictOfflinePageUrl(url);
  notify_status_change_.Run(url, false);
}

void FeedOfflineHost::CacheOfflinePageUrlAndId(const std::string& url,
                                               int64_t id) {
  url_hash_to_id_[base::FastHash(url)] = id;
}

void FeedOfflineHost::EvictOfflinePageUrl(const std::string& url) {
  url_hash_to_id_.erase(base::FastHash(url));
}

}  // namespace feed
