// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_CAPTION_BUBBLE_SESSION_OBSERVER_H_
#define COMPONENTS_LIVE_CAPTION_CAPTION_BUBBLE_SESSION_OBSERVER_H_

#include "base/functional/callback.h"

namespace captions {

using EndSessionCallback = base::RepeatingCallback<void(const std::string&)>;

class CaptionBubbleSessionObserver {
 public:
  CaptionBubbleSessionObserver() = default;
  virtual ~CaptionBubbleSessionObserver() = default;
  CaptionBubbleSessionObserver(const CaptionBubbleSessionObserver&) = delete;
  CaptionBubbleSessionObserver& operator=(const CaptionBubbleSessionObserver&) =
      delete;

  virtual void SetEndSessionCallback(EndSessionCallback callback) = 0;
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_CAPTION_BUBBLE_SESSION_OBSERVER_H_
