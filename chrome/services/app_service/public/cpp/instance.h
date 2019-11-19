// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_H_
#define CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_H_

#include <memory>
#include <string>

#include "base/time/time.h"

namespace aura {
class Window;
}

namespace apps {

enum InstanceState {
  kUnknown = 0,
  kStarted = 0x01,
  kRunning = 0x02,
  kActive = 0x04,
  kVisible = 0x08,
  kDestroyed = 0x80,
};

// Instance is used to represent an App Instance, or a running app.
class Instance {
 public:
  Instance(const std::string& app_id, aura::Window* window);
  ~Instance();

  Instance(const Instance&) = delete;
  Instance& operator=(const Instance&) = delete;

  std::unique_ptr<Instance> Clone();

  void SetLaunchId(const std::string& launch_id) { launch_id_ = launch_id; }
  void UpdateState(InstanceState state, const base::Time& last_updated_time);

  const std::string& AppId() const { return app_id_; }
  aura::Window* Window() const { return window_; }
  const std::string& LaunchId() const { return launch_id_; }
  InstanceState State() const { return state_; }
  const base::Time& LastUpdatedTime() const { return last_updated_time_; }

 private:
  std::string app_id_;

  // window_ is owned by ash and will be deleted when the user closes the
  // window. Instance itself doesn't observe the window. The window's observer
  // is responsible to delete Instance from InstanceRegistry when the window is
  // destroyed.
  aura::Window* window_;
  std::string launch_id_;
  InstanceState state_;
  base::Time last_updated_time_;
};

}  // namespace apps

#endif  // CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_H_
