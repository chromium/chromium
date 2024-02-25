// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEEP_ALIVE_REGISTRY_KEEP_ALIVE_REGISTRY_H_
#define COMPONENTS_KEEP_ALIVE_REGISTRY_KEEP_ALIVE_REGISTRY_H_

#include <unordered_map>
#include <vector>

#include "base/memory/singleton.h"
#include "base/observer_list.h"

enum class KeepAliveOrigin;
enum class KeepAliveRestartOption;
class KeepAliveStateObserver;

// Centralized registry that objects in the browser can use to
// express their requirements wrt the lifetime of the browser.
// Observers registered with it can then react as those requirements
// change.
// In particular, this is how the browser process knows when to stop
// or stay alive.
// Note: BrowserProcessImpl registers to react on changes.
// TestingBrowserProcess does not do it, meaning that the shutdown
// sequence does not happen during unit tests.
// Note: This is not thread-safe, and should only be used on the main thread.
class KeepAliveRegistry {
 public:
  static KeepAliveRegistry* GetInstance();

  KeepAliveRegistry(const KeepAliveRegistry&) = delete;
  KeepAliveRegistry& operator=(const KeepAliveRegistry&) = delete;

  // Methods to query the state of the registry.
  bool IsKeepingAlive() const;
  bool IsKeepingAliveOnlyByBrowserOrigin() const;
  bool IsRestartAllowed() const;
  bool IsOriginRegistered(KeepAliveOrigin origin) const;

  void AddObserver(KeepAliveStateObserver* observer);
  void RemoveObserver(KeepAliveStateObserver* observer);

  // Returns whether restart would be allowed if all the keep alives for the
  // provided |origins| were not registered.
  bool WouldRestartWithout(const std::vector<KeepAliveOrigin>& origins) const;

  // True if shutdown is in progress. No new KeepAlive should be registered
  // while shutting down.
  bool IsShuttingDown() const;
  // Call when shutting down to ensure registering a new KeepAlive CHECKs.
  void SetIsShuttingDown(bool value = true);

  // True if restarting is in progress.
  bool IsRestarting() const;

  // Called when restarting is triggered.
  void SetRestarting();

 private:
  friend struct base::DefaultSingletonTraits<KeepAliveRegistry>;
  // Friend to be able to use Register/Unregister
  friend class ScopedKeepAlive;
  friend std::ostream& operator<<(std::ostream& out,
                                  const KeepAliveRegistry& registry);

  // TODO(dgn): Remove this when std::hash supports enums directly (c++14)
  // http://www.open-std.org/jtc1/sc22/wg21/docs/lwg-defects.html#2148
  struct EnumClassHash {
    std::size_t operator()(KeepAliveOrigin origin) const {
      return static_cast<int>(origin);
    }
  };

  using OriginMap = std::unordered_map<KeepAliveOrigin, int, EnumClassHash>;

  KeepAliveRegistry();
  ~KeepAliveRegistry();

  // Add/Remove entries. Do not use directly, use ScopedKeepAlive instead.
  void Register(KeepAliveOrigin origin, KeepAliveRestartOption restart);
  void Unregister(KeepAliveOrigin origin, KeepAliveRestartOption restart);

  // Methods called when a specific aspect of the state of the registry changes.
  void OnKeepAliveStateChanged(bool new_keeping_alive);
  void OnRestartAllowedChanged(bool new_restart_allowed);

  // Unregisters one occurrence of the provided |origin| from |keep_alive_map|
  void DecrementCount(KeepAliveOrigin origin, OriginMap* keep_alive_map);

  // Tracks the registered KeepAlives, storing the origin and the number of
  // registered KeepAlives for each.
  OriginMap registered_keep_alives_;

  // Tracks the registered KeepAlives that had KeepAliveRestartOption::ENABLED
  // set, storing the origin and the number of restart allowed KeepAlives for
  // each origin.
  OriginMap restart_allowed_keep_alives_;

  // Total number of registered KeepAlives
  int registered_count_;

  // Number of registered keep alives that have KeepAliveRestartOption::ENABLED.
  int restart_allowed_count_;

  // Used to guard against registering during shutdown.
  bool is_shutting_down_ = false;

  // Used to handle KeepAliveRestartOption::ENABLED.
  bool is_restarting_ = false;

  base::ObserverList<KeepAliveStateObserver>::Unchecked observers_;
};

#endif  // COMPONENTS_KEEP_ALIVE_REGISTRY_KEEP_ALIVE_REGISTRY_H_
