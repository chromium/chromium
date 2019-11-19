// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_contents.h"

namespace chromecast {

CastWebContents::Observer::Observer() : cast_web_contents_(nullptr) {}

CastWebContents::Observer::~Observer() {
  if (cast_web_contents_) {
    cast_web_contents_->RemoveObserver(this);
  }
}

void CastWebContents::Observer::Observe(CastWebContents* cast_web_contents) {
  if (cast_web_contents == cast_web_contents_) {
    // Early exit to avoid infinite loops if we're in the middle of a callback.
    return;
  }
  if (cast_web_contents_) {
    cast_web_contents_->RemoveObserver(this);
  }
  cast_web_contents_ = cast_web_contents;
  if (cast_web_contents_) {
    cast_web_contents_->AddObserver(this);
  }
}

void CastWebContents::Observer::ResetCastWebContents() {
  cast_web_contents_->RemoveObserver(this);
  cast_web_contents_ = nullptr;
}

CastWebContents::InitParams::InitParams() = default;
CastWebContents::InitParams::InitParams(const InitParams& other) = default;
CastWebContents::InitParams::~InitParams() = default;

}  // namespace chromecast
