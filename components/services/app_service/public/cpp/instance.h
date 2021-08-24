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
    // Create an InstanceKey for an app instance backed by a WebContents.
    static InstanceKey ForWebBasedApp(aura::Window* window);

    // Create an InstanceKey for any non-web app type.
    static InstanceKey ForWindowBasedApp(aura::Window* window);

    InstanceKey(const InstanceKey& instance_key) = default;
    InstanceKey(InstanceKey&& instance_key) = default;
    ~InstanceKey() = default;

    // Return enclosing app windows for the |app_id|. If the app is in a browser
    // tab, the window returned will be the window of the browser.
    aura::Window* GetEnclosingAppWindow() const;
    bool IsValid() const { return window_ != nullptr; }
    bool operator<(const InstanceKey& other) const;
    bool operator==(const InstanceKey& other) const;
    bool operator!=(const InstanceKey& other) const;
    InstanceKey& operator=(InstanceKey&&) = default;

    bool IsForWebBasedApp() const { return is_web_contents_backed_; }

    friend struct InstanceKeyHash;
    friend std::ostream& operator<<(
        std::ostream& os,
        const apps::Instance::InstanceKey& instance_key);

   private:
    explicit InstanceKey(aura::Window* window, bool is_web_contents_backed);

    // window_ is owned by ash and will be deleted when the user closes the
    // window. Instance itself doesn't observe the window. The window's observer
    // is responsible to delete Instance from InstanceRegistry when the window
    // is destroyed.
    aura::Window* window_;

    // Whether the app is a WebContents backed app. Will eventually be replaced
    // by an ID representing the WebContents which may live remotely.
    bool is_web_contents_backed_;
  };

  Instance(const std::string& app_id, InstanceKey&& instance_key);
  ~Instance();

  Instance(const Instance&) = delete;
  Instance& operator=(const Instance&) = delete;

  std::unique_ptr<Instance> Clone();

  void SetLaunchId(const std::string& launch_id) { launch_id_ = launch_id; }
  void UpdateState(InstanceState state, const base::Time& last_updated_time);
  void SetBrowserContext(content::BrowserContext* browser_context);

  const std::string& AppId() const { return app_id_; }
  const InstanceKey& GetInstanceKey() const { return instance_key_; }
  const std::string& LaunchId() const { return launch_id_; }
  InstanceState State() const { return state_; }
  const base::Time& LastUpdatedTime() const { return last_updated_time_; }
  content::BrowserContext* BrowserContext() const { return browser_context_; }

 private:
  std::string app_id_;
  InstanceKey instance_key_;
  std::string launch_id_;
  InstanceState state_;
  base::Time last_updated_time_;
  content::BrowserContext* browser_context_ = nullptr;
};

std::ostream& operator<<(std::ostream& os,
                         const apps::Instance::InstanceKey& instance_key);

struct InstanceKeyHash {
  size_t operator()(const apps::Instance::InstanceKey& key) const;
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_H_
