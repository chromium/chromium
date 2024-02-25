// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_H_

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
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
// `instance_id_` is the unique id for instance. For any two instances, if the
// instance id is the same, the app id must be the same, well, the window might
// be different. For example, When a web app opened in tab is pulled to a new
// Lacros window, the window might be changed. Instance should exist on Ash side
// only.
class Instance {
 public:
  Instance(const std::string& app_id,
           const base::UnguessableToken& instance_id,
           aura::Window* window);

  Instance(const Instance&) = delete;
  Instance& operator=(const Instance&) = delete;
  ~Instance();

  std::unique_ptr<Instance> Clone();

  void SetLaunchId(const std::string& launch_id) { launch_id_ = launch_id; }
  void UpdateState(InstanceState state, const base::Time& last_updated_time);
  void SetBrowserContext(content::BrowserContext* browser_context) {
    browser_context_ = browser_context;
  }
  void SetWindow(aura::Window* window) { window_ = window; }

  const std::string& AppId() const { return app_id_; }
  const base::UnguessableToken& InstanceId() const { return instance_id_; }
  aura::Window* Window() const { return window_; }
  const std::string& LaunchId() const { return launch_id_; }
  InstanceState State() const { return state_; }
  const base::Time& LastUpdatedTime() const { return last_updated_time_; }
  content::BrowserContext* BrowserContext() const { return browser_context_; }

 private:
  friend class InstanceRegistry;
  friend class InstanceTest;

  const std::string app_id_;

  // The unique id for instance.
  base::UnguessableToken instance_id_;

  raw_ptr<aura::Window, DanglingUntriaged> window_ = nullptr;

  std::string launch_id_;
  InstanceState state_ = InstanceState::kUnknown;
  base::Time last_updated_time_;
  raw_ptr<content::BrowserContext> browser_context_ = nullptr;
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_H_
