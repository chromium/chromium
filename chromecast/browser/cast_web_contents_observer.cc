// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_contents_observer.h"

namespace chromecast {

CastWebContentsObserver::CastWebContentsObserver() = default;

CastWebContentsObserver::~CastWebContentsObserver() = default;

void CastWebContentsObserver::Observe(
    mojom::CastWebContents* cast_web_contents) {
  if (receiver_.is_bound()) {
    receiver_.reset();
  }
  if (cast_web_contents) {
    cast_web_contents->AddObserver(receiver_.BindNewPipeAndPassRemote());
  }
}

}  // namespace chromecast
