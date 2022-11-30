// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_UNREAD_CONTENT_OBSERVER_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_UNREAD_CONTENT_OBSERVER_H_

#include "base/memory/weak_ptr.h"

namespace feed {

// Observes whether there is unread content for a specific stream type.
// In some cases, this information will not be known until after stream
// data is loaded from the database. This observer will not be notified until
// the information is available.
class UnreadContentObserver {
 public:
  UnreadContentObserver();
  virtual ~UnreadContentObserver();
  virtual void HasUnreadContentChanged(bool has_unread_content) = 0;
  UnreadContentObserver(const UnreadContentObserver&) = delete;
  UnreadContentObserver& operator=(const UnreadContentObserver&) = delete;

  base::WeakPtr<UnreadContentObserver> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<UnreadContentObserver> weak_ptr_factory_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_UNREAD_CONTENT_OBSERVER_H_
