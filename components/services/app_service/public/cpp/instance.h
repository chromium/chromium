// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_H_

#include <memory>
#include <string>
#include <utility>

#include "base/time/time.h"
#include "content/public/browser/browser_context.h"
#include "ui/aura/window.h"

namespace apps {

enum InstanceState {
  kUnknown = 0,
  kStarted = 0x01,
  kRunning = 0x02,
  kActive = 0x04,
  kVisible = 0x08,
  kHidden = 0x10,
  kDestroyed = 0x80,
};

// Instance is used to represent an App Instance, or a running app.
class Instance {
 public:
  // InstanceKey is the unique key for the instance.
  class InstanceKey {
   public:
    explicit InstanceKey(aura::Window* window);
    ~InstanceKey() = default;
    aura::Window* Window() const { return window_; }
    bool operator<(const InstanceKey& other) const;
    bool operator==(const InstanceKey& other) const;
    bool operator!=(const InstanceKey& other) const;

   private:
    // window_ is owned by ash and will be deleted when the user closes the
    // window. Instance itself doesn't observe the window. The window's observer
    // is responsible to delete Instance from InstanceRegistry when the window
    // is destroyed.
    aura::Window* window_;
  };

  Instance(const std::string& app_id,
           std::unique_ptr<InstanceKey> instance_key);
  ~Instance();

  Instance(const Instance&) = delete;
  Instance& operator=(const Instance&) = delete;

  std::unique_ptr<Instance> Clone();

  void SetLaunchId(const std::string& launch_id) { launch_id_ = launch_id; }
  void UpdateState(InstanceState state, const base::Time& last_updated_time);
  void SetBrowserContext(content::BrowserContext* browser_context);

  const std::string& AppId() const { return app_id_; }
  const InstanceKey& GetInstanceKey() const { return *instance_key_; }
  aura::Window* Window() const { return instance_key_->Window(); }
  const std::string& LaunchId() const { return launch_id_; }
  InstanceState State() const { return state_; }
  const base::Time& LastUpdatedTime() const { return last_updated_time_; }
  content::BrowserContext* BrowserContext() const { return browser_context_; }

 private:
  std::string app_id_;
  std::unique_ptr<InstanceKey> instance_key_;
  std::string launch_id_;
  InstanceState state_;
  base::Time last_updated_time_;
  content::BrowserContext* browser_context_ = nullptr;
};

}  // namespace apps

std::ostream& operator<<(std::ostream& os,
                         const apps::Instance::InstanceKey& instance_key);

struct InstanceKeyHash {
  size_t operator()(const apps::Instance::InstanceKey& key) const;
};

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_H_
