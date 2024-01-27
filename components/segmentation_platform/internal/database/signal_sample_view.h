// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_SAMPLE_VIEW_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_SAMPLE_VIEW_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/public/proto/types.pb.h"

namespace segmentation_platform {

// A container "view" on top of all the signal database sample list, that helps
// to query a particular histogram / user action or enum histograms.
class SignalSampleView {
 public:
  using Entries = std::vector<SignalDatabase::DbEntry>;

  // Object to query a particular signal to be included.
  struct Query {
    Query(proto::SignalType type,
          uint64_t metric_hash,
          base::Time start,
          base::Time end,
          const std::vector<int>& enum_ids = {});
    ~Query();
    Query(const Query&);

    // Returns true if the `sample` matches the view query.
    bool IsFeatureMatching(const SignalDatabase::DbEntry& sample) const;

    // The metric type and hash to filter in.
    const proto::SignalType type = proto::SignalType::UNKNOWN_SIGNAL_TYPE;
    const uint64_t metric_hash = 0;
    // The time period to filter in, start and end inclusive.
    const base::Time start;
    const base::Time end;
    // List of enum values to include. If empty, then all signals are included.
    const std::vector<int> enum_ids;
  };

  // Use begin() to create iterator, and ++ operator to increment the iterator.
  struct Iterator {
    Iterator(const SignalSampleView* view, size_t current);
    ~Iterator();
    Iterator(Iterator& other);

    const SignalDatabase::DbEntry& operator*() { return samples_[current_]; }
    const SignalDatabase::DbEntry* operator->() { return &samples_[current_]; }

    Iterator& operator++() {
      current_ = view_->FindNext(current_);
      return *this;
    }
    bool operator==(const Iterator& other) const {
      CHECK_EQ(view_, other.view_);
      return current_ == other.current_;
    }

    // Current index the iterator points to. Prefer to use * operator instead:
    // `*it`.
    size_t current() const { return current_; }

   private:
    const raw_ptr<const SignalSampleView> view_;
    const Entries& samples_;
    size_t current_;
  };

  // Creates a view on the database samples. `query` specifies the filter to
  // apply on the samples for the view. If `query` is empty, all samples are
  // included, just acts like a vector.
  SignalSampleView(const std::vector<SignalDatabase::DbEntry>& samples,
                   const std::optional<Query>& query);
  ~SignalSampleView();
  SignalSampleView(const SignalSampleView&) = delete;

  // C++ iterable object methods.
  Iterator begin() const { return Iterator(this, FindNext(-1)); }
  Iterator end() const { return Iterator(this, EndIndex()); }

  // Returns the last element in the view, if view is empty, returns end().
  Iterator Last() const { return Iterator(this, FindPrev(samples_.size())); }

  // The empty() and size() methods take O(n) time, on the size of the samples.
  bool empty() const { return size() == 0; }
  size_t size() const;

 private:
  // Returns the index that represents the "end()" iterator of this object.
  size_t EndIndex() const { return samples_.size(); }

  // Finds the next index that matches the given query.
  size_t FindNext(int index) const;

  // Finds the previous index that matches the given query.
  size_t FindPrev(int index) const;

  const std::vector<SignalDatabase::DbEntry>& samples_;
  std::optional<Query> query_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_SAMPLE_VIEW_H_
