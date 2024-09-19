// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/new_frame_catcher.h"

namespace autofill::test {

NewFrameCatcher::NewFrameCatcher(web::WebFramesManager* manager) {
  scoped_observer_.Observe(manager);
}

NewFrameCatcher::~NewFrameCatcher() = default;

void NewFrameCatcher::WebFrameBecameAvailable(
    web::WebFramesManager* web_frames_manager,
    web::WebFrame* web_frame) {
  latest_new_frame_ = web_frame;
}

}  // namespace autofill::test
