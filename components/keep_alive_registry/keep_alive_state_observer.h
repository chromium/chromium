// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEEP_ALIVE_REGISTRY_KEEP_ALIVE_STATE_OBSERVER_H_
#define COMPONENTS_KEEP_ALIVE_REGISTRY_KEEP_ALIVE_STATE_OBSERVER_H_

class KeepAliveStateObserver {
 public:
  virtual void OnKeepAliveStateChanged(bool is_keeping_alive) = 0;
  virtual void OnKeepAliveRestartStateChanged(bool can_restart) = 0;

 protected:
  virtual ~KeepAliveStateObserver() = default;
};

#endif  // COMPONENTS_KEEP_ALIVE_REGISTRY_KEEP_ALIVE_STATE_OBSERVER_H_
