// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream/unread_content_notifier.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

namespace feed {
namespace feed_stream {

UnreadContentNotifier::~UnreadContentNotifier() = default;
UnreadContentNotifier::UnreadContentNotifier(UnreadContentNotifier&&) = default;
UnreadContentNotifier& UnreadContentNotifier::operator=(
    UnreadContentNotifier&&) = default;

UnreadContentNotifier::UnreadContentNotifier(
    base::WeakPtr<UnreadContentObserver> observer)
    : observer_(observer) {}

void UnreadContentNotifier::NotifyIfValueChanged(bool has_unread_content) {
  bool changed = !is_initialized_ || has_unread_content_ != has_unread_content;
  has_unread_content_ = has_unread_content;
  is_initialized_ = true;
  if (changed) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&UnreadContentObserver::HasUnreadContentChanged,
                       observer_, has_unread_content));
  }
}

}  // namespace feed_stream
}  // namespace feed
