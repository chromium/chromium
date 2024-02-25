// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/signal_sample_view.h"

#include "base/containers/contains.h"

namespace segmentation_platform {

bool SignalSampleView::Query::IsFeatureMatching(
    const SignalDatabase::DbEntry& sample) const {
  return sample.type == type && sample.name_hash == metric_hash &&
         start <= sample.time && end >= sample.time &&
         (enum_ids.empty() || base::Contains(enum_ids, sample.value));
}

SignalSampleView::Query::Query(proto::SignalType type,
                               uint64_t metric_hash,
                               base::Time start,
                               base::Time end,
                               const std::vector<int>& enum_ids)
    : type(type),
      metric_hash(metric_hash),
      start(start),
      end(end),
      enum_ids(enum_ids) {
  DCHECK(enum_ids.empty() || type == proto::SignalType::HISTOGRAM_ENUM);
}

SignalSampleView::Query::~Query() = default;
SignalSampleView::Query::Query(const Query& query) = default;

SignalSampleView::Iterator::Iterator(const SignalSampleView* view,
                                     size_t current)
    : view_(view), samples_(view_->samples_), current_(current) {}
SignalSampleView::Iterator::~Iterator() = default;
SignalSampleView::Iterator::Iterator(Iterator& other) = default;

SignalSampleView::SignalSampleView(
    const std::vector<SignalDatabase::DbEntry>& samples,
    const std::optional<Query>& query)
    : samples_(samples), query_(query) {}
SignalSampleView::~SignalSampleView() = default;

size_t SignalSampleView::FindNext(int index) const {
  while (true) {
    ++index;
    if (static_cast<unsigned>(index) >= samples_.size()) {
      return EndIndex();
    }
    if (!query_ || query_->IsFeatureMatching(samples_[index])) {
      return index;
    }
  }
}

size_t SignalSampleView::FindPrev(int index) const {
  if (static_cast<unsigned>(index) > samples_.size()) {
    index = samples_.size();
  }
  while (true) {
    --index;
    if (index < 0) {
      return EndIndex();
    }
    if (!query_ || query_->IsFeatureMatching(samples_[index])) {
      return index;
    }
  }
}

size_t SignalSampleView::size() const {
  if (!query_) {
    return samples_.size();
  }
  size_t i = 0;
  for (auto it = begin(); it != end(); ++it, ++i) {
  }
  return i;
}

}  // namespace segmentation_platform
