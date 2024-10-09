// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_event_global_tracker.h"

#include "base/memory/singleton.h"

OmniboxEventGlobalTracker* OmniboxEventGlobalTracker::GetInstance() {
  return base::Singleton<OmniboxEventGlobalTracker>::get();
}

base::CallbackListSubscription OmniboxEventGlobalTracker::RegisterCallback(
    const OnURLOpenedCallback& cb) {
  return on_url_opened_callback_list_.Add(cb);
}

void OmniboxEventGlobalTracker::OnURLOpened(OmniboxLog* log) {
  on_url_opened_callback_list_.Notify(log);
}

OmniboxEventGlobalTracker::OmniboxEventGlobalTracker() = default;

OmniboxEventGlobalTracker::~OmniboxEventGlobalTracker() = default;
