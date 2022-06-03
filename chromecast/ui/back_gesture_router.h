// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_UI_BACK_GESTURE_ROUTER_H_
#define CHROMECAST_UI_BACK_GESTURE_ROUTER_H_

namespace chromecast {

// Helper class for exposing gesture events to a remote process.
class BackGestureRouter {
 public:
  class Delegate {
   public:
    virtual void SetCanGoBack(bool can_go_back) = 0;
  };
  virtual void SetBackGestureDelegate(Delegate* delegate) = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_UI_BACK_GESTURE_ROUTER_H_
