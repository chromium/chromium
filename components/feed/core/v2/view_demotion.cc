// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/view_demotion.h"

#include <algorithm>
#include <map>
#include <ostream>
#include <tuple>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/feed_feature_list.h"

namespace feed {
namespace {
base::TimeDelta kMaxViewAge = base::Hours(72);

bool IsEnabled(FeedStream& feed_stream) {
  return base::FeatureList::IsEnabled(kFeedSignedOutViewDemotion) &&
         !feed_stream.IsSignedIn();
}
}  // namespace

namespace internal {
DocViewDigest CreateDigest(std::vector<feedstore::DocView> all_views) {
  const base::Time now = base::Time::Now();
  const base::Time too_old = now - kMaxViewAge;
  DocViewDigest digest;

  // Remove all views that are too old.
  std::erase_if(all_views, [&](const feedstore::DocView& view) {
    base::Time view_time =
        feedstore::FromTimestampMillis(view.view_time_millis());
    if (view_time > now || view_time < too_old) {
      digest.old_doc_views.push_back(view);
      return true;
    }
    return false;
  });

  // Note that all_views will be sorted by the datastore key, which is prefixed
  // with the docid, and therefore we can assume that all views for a given
  // docid are adjacent in this list. We take advantage of this here.
  for (const feedstore::DocView& view : all_views) {
    if (digest.doc_view_counts.empty() ||
        digest.doc_view_counts.back().docid != view.docid()) {
      digest.doc_view_counts.push_back(DocViewCount{view.docid(), 1});
    } else {
      ++digest.doc_view_counts.back().view_count;
    }
  }

  // If there are too many recent entries, remove docids which have been viewed
  // least recently.
  if (digest.doc_view_counts.size() > GetFeedConfig().max_docviews_to_send) {
    // Get the most recent view time for each doc, sort by time and select the
    // docids to remove.
    size_t remove_count =
        digest.doc_view_counts.size() - GetFeedConfig().max_docviews_to_send;

    base::UmaHistogramCounts100(
        "ContentSuggestions.Feed.DroppedDocumentViewCount", remove_count);

    std::vector<std::pair<int64_t, uint64_t>> latest_view_to_docid;
    for (const feedstore::DocView& view : all_views) {
      if (latest_view_to_docid.empty() ||
          latest_view_to_docid.back().second != view.docid()) {
        latest_view_to_docid.emplace_back(view.view_time_millis(),
                                          view.docid());
      } else {
        auto& view_time = latest_view_to_docid.back().first;
        view_time = std::max(view_time, view.view_time_millis());
      }
    }

    base::ranges::sort(latest_view_to_docid);

    std::vector<uint64_t> docids_to_remove;
    docids_to_remove.reserve(remove_count);
    for (auto iter = latest_view_to_docid.begin();
         iter != latest_view_to_docid.begin() + remove_count; ++iter) {
      docids_to_remove.push_back(iter->second);
    }

    // Finally, remove the old docids, and add to old_doc_views.
    base::flat_set<uint64_t> docids_to_remove_set(std::move(docids_to_remove));
    std::erase_if(digest.doc_view_counts, [&](const DocViewCount& view_count) {
      return docids_to_remove_set.contains(view_count.docid);
    });
    for (const feedstore::DocView& view : all_views) {
      if (docids_to_remove_set.contains(view.docid())) {
        digest.old_doc_views.push_back(view);
      }
    }
    DCHECK_EQ(digest.doc_view_counts.size(),
              GetFeedConfig().max_docviews_to_send);
  }

  base::UmaHistogramCounts100(
      "ContentSuggestions.Feed.DocumentViewSendCount100",
      digest.doc_view_counts.size());

  base::UmaHistogramCounts1000(
      "ContentSuggestions.Feed.DocumentViewSendCount1000",
      digest.doc_view_counts.size());
  return digest;
}
}  // namespace internal

bool DocViewCount::operator==(const DocViewCount& rhs) const {
  return std::tie(docid, view_count) == std::tie(rhs.docid, rhs.view_count);
}

void ReadDocViewDigestIfEnabled(
    FeedStream& feed_stream,
    base::OnceCallback<void(DocViewDigest)> callback) {
  if (!IsEnabled(feed_stream)) {
    std::move(callback).Run({});
    return;
  }
  auto callback_wrapper = [](base::OnceCallback<void(DocViewDigest)> callback,
                             std::vector<feedstore::DocView> all_views) {
    std::move(callback).Run(internal::CreateDigest(std::move(all_views)));
  };
  feed_stream.GetStore().ReadDocViews(
      base::BindOnce(callback_wrapper, std::move(callback)));
}

void RemoveOldDocViews(const DocViewDigest& doc_view_digest,
                       FeedStore& feed_store) {
  feed_store.RemoveDocViews(doc_view_digest.old_doc_views);
}

void WriteDocViewIfEnabled(FeedStream& feed_stream, uint64_t docid) {
  if (!IsEnabled(feed_stream)) {
    return;
  }
  feed_stream.GetStore().WriteDocView(feedstore::CreateDocView(docid));
}

DocViewDigest::DocViewDigest() = default;
DocViewDigest::~DocViewDigest() = default;
DocViewDigest::DocViewDigest(const DocViewDigest&) = default;
DocViewDigest::DocViewDigest(DocViewDigest&&) = default;
DocViewDigest& DocViewDigest::operator=(const DocViewDigest&) = default;
DocViewDigest& DocViewDigest::operator=(DocViewDigest&&) = default;

std::ostream& operator<<(std::ostream& os, const DocViewCount& doc_view_count) {
  return os << "DocViewCount{docid: " << doc_view_count.docid
            << " count: " << doc_view_count.view_count << " }";
}

}  // namespace feed
