// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/media_routes_observer.h"

#include "base/check.h"
#include "components/media_router/browser/media_router.h"

namespace media_router {

MediaRoutesObserver::MediaRoutesObserver(MediaRouter* router,
                                         const MediaSource::Id& source_id)
    : router_(router), source_id_(source_id) {
  DCHECK(router_);
  router_->RegisterMediaRoutesObserver(this);
}

MediaRoutesObserver::~MediaRoutesObserver() {
  router_->UnregisterMediaRoutesObserver(this);
}

}  // namespace media_router
