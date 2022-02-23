// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_WATCHER_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_WATCHER_H_

namespace chromecast {

class RuntimeApplication;

// This class is responsible for providing a callback when the current runtime
// application changes.
class RuntimeApplicationWatcher {
 public:
  virtual ~RuntimeApplicationWatcher();

  // Called when the current runtime application changes, with |application|
  // being a pointer to this instance or nullptr if no such instance exists.
  virtual void OnRuntimeApplicationChanged(RuntimeApplication* application) = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_WATCHER_H_
