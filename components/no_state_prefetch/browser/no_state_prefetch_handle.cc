// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"

#include <algorithm>

#include "base/check_op.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;

namespace prerender {

NoStatePrefetchHandle::Observer::Observer() {}

NoStatePrefetchHandle::Observer::~Observer() {}

NoStatePrefetchHandle::~NoStatePrefetchHandle() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (prefetch_data_) {
    prefetch_data_->contents()->RemoveObserver(this);
  }
}

void NoStatePrefetchHandle::SetObserver(Observer* observer) {
  observer_ = observer;
}

void NoStatePrefetchHandle::OnNavigateAway() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (prefetch_data_)
    prefetch_data_->OnHandleNavigatedAway(this);
}

void NoStatePrefetchHandle::OnCancel() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (prefetch_data_)
    prefetch_data_->OnHandleCanceled(this);
}

bool NoStatePrefetchHandle::IsPrefetching() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return prefetch_data_.get() != nullptr &&
         !prefetch_data_->contents()->prefetching_has_been_cancelled();
}

bool NoStatePrefetchHandle::IsFinishedLoading() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return prefetch_data_ && prefetch_data_->contents()->has_finished_loading();
}

bool NoStatePrefetchHandle::IsAbandoned() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return prefetch_data_ && !prefetch_data_->abandon_time().is_null();
}

NoStatePrefetchContents* NoStatePrefetchHandle::contents() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return prefetch_data_ ? prefetch_data_->contents() : nullptr;
}

const GURL& NoStatePrefetchHandle::prefetch_url() const {
  return prefetch_url_;
}

NoStatePrefetchHandle::NoStatePrefetchHandle(
    NoStatePrefetchManager::NoStatePrefetchData* prefetch_data)
    : observer_(nullptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (prefetch_data) {
    prefetch_data_ = prefetch_data->AsWeakPtr();
    prefetch_data->OnHandleCreated(this);
    if (prefetch_data->contents()) {
      prefetch_url_ = prefetch_data->contents()->prefetch_url();
    }
  }
}

void NoStatePrefetchHandle::OnPrefetchStop(
    NoStatePrefetchContents* no_state_prefetch_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (observer_)
    observer_->OnPrefetchStop(this);
}

void NoStatePrefetchHandle::OnPrefetchNetworkBytesChanged(
    NoStatePrefetchContents* no_state_prefetch_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (observer_)
    observer_->OnPrefetchNetworkBytesChanged(this);
}

bool NoStatePrefetchHandle::RepresentingSamePrefetchAs(
    NoStatePrefetchHandle* other) const {
  return other && other->prefetch_data_ && prefetch_data_ &&
         prefetch_data_.get() == other->prefetch_data_.get();
}

}  // namespace prerender
