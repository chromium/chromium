// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_FEED_CORE_V2_VIEW_DEMOTION_H_
#define COMPONENTS_FEED_CORE_V2_VIEW_DEMOTION_H_

#include <stdint.h>

#include <iosfwd>
#include <vector>
#include "base/functional/callback_forward.h"

namespace feedstore {
class DocView;
}
namespace feed {
struct DocViewDigest;

namespace internal {
DocViewDigest CreateDigest(std::vector<feedstore::DocView> all_views);
}

class FeedStream;

struct DocViewCount {
  // Uniquely identifies content.
  uint64_t docid = 0;
  // Number of views.
  int64_t view_count = 0;

  bool operator==(const DocViewCount& rhs) const;
};

std::ostream& operator<<(std::ostream& os, const DocViewCount& doc_view_count);

// Summarizes stored document view data.
struct DocViewDigest {
  DocViewDigest();
  ~DocViewDigest();
  DocViewDigest(const DocViewDigest&);
  DocViewDigest(DocViewDigest&&);
  DocViewDigest& operator=(const DocViewDigest&);
  DocViewDigest& operator=(DocViewDigest&&);

  // Document views which are not expired.
  std::vector<DocViewCount> doc_view_counts;
  // Document views which are expired, and should be removed.
  std::vector<feedstore::DocView> old_doc_views;
};

// Reads the DocViewDigest if view demotion is enabled. Otherwise, `callback` is
// called immediately with an empty digest.
void ReadDocViewDigestIfEnabled(
    FeedStream& feed_stream,
    base::OnceCallback<void(DocViewDigest)> callback);

// Records a document view if view demotion is enabled.
void WriteDocViewIfEnabled(FeedStream& feed_stream, uint64_t docid);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_VIEW_DEMOTION_H_
