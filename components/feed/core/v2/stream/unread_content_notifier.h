// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_STREAM_UNREAD_CONTENT_NOTIFIER_H_
#define COMPONENTS_FEED_CORE_V2_STREAM_UNREAD_CONTENT_NOTIFIER_H_

#include "components/feed/core/v2/public/unread_content_observer.h"

namespace feed {
namespace feed_stream {

// Wraps and notifies a `UnreadContentObserver`.
class UnreadContentNotifier {
 public:
  explicit UnreadContentNotifier(base::WeakPtr<UnreadContentObserver> observer);
  ~UnreadContentNotifier();
  UnreadContentNotifier(UnreadContentNotifier&&);
  UnreadContentNotifier& operator=(UnreadContentNotifier&&);
  UnreadContentNotifier(const UnreadContentNotifier&) = delete;
  UnreadContentNotifier& operator=(const UnreadContentNotifier&) = delete;

  // Notify the observer if it `has_unread_content` has changed.
  void NotifyIfValueChanged(bool has_unread_content);

  base::WeakPtr<UnreadContentObserver> observer() const { return observer_; }

 private:
  base::WeakPtr<UnreadContentObserver> observer_;
  bool is_initialized_ = false;
  bool has_unread_content_ = false;
};

}  // namespace feed_stream
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_STREAM_UNREAD_CONTENT_NOTIFIER_H_
