// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TIPS_MANAGER_TIPS_MANAGER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TIPS_MANAGER_TIPS_MANAGER_H_

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefRegistrySimple;
class PrefService;
namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace segmentation_platform {

enum class TipIdentifier;
enum class TipPresentationContext;

// The `TipsManager` is a `KeyedService` responsible for managing and
// coordinating in-product tips within Chrome. It provides a common
// interface for:
//
// - Tracking user interactions and relevant signals.
// - Providing data to the Segmentation Platform for tip selection.
//
// This class is designed to be extended by platform-specific implementations
// (e.g., `IOSChromeTipsManager`) that handle the actual presentation and
// interaction logic for tips within their respective environments.
class TipsManager : public KeyedService {
 public:
  // Constructor.
  explicit TipsManager(PrefService* pref_service,
                       PrefService* local_pref_service);

  TipsManager(const TipsManager&) = delete;
  TipsManager& operator=(const TipsManager&) = delete;

  ~TipsManager() override;

  // Registers all preferences used by the `TipsManager` that will be attached
  // to a profile.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Registers all preferences used by the `TipsManager` that will be
  // stored in the local state, not attached to any profile.
  static void RegisterLocalPrefs(PrefRegistrySimple* registry);

  // `KeyedService` implementation. Performs cleanup and resource release before
  // the service is destroyed.
  void Shutdown() override;

  // Called when a user interacts with a displayed `tip`.
  //
  // `tip`: The identifier of the interacted tip.
  // `context`: The context in which the tip was presented.
  //
  // This method is responsible for processing the interaction and performing
  // any necessary actions, such as:
  //
  // - Updating tip state or metrics.
  // - Triggering related actions (e.g., opening a URL, showing a dialog).
  // - Dismissing the tip.
  //
  // This is a pure virtual function and must be implemented by derived classes
  // to provide platform-specific interaction handling.
  virtual void HandleInteraction(TipIdentifier tip,
                                 TipPresentationContext context) = 0;

  // Notifies the Tips Manager about an observed `signal` event. Returns `true`
  // if the `signal` was properly handled by the Tips Manager.
  //
  // This triggers:
  //
  // 1. Internal state updates for relevant Tip(s).
  // 2. Recording of the signal in UMA histograms.
  // 3. Persistence of the signal data in Prefs for future use.
  bool NotifySignal(std::string_view signal);

  // Returns `true` if the given `signal` has ever been fired, or `false`
  // otherwise.
  bool WasSignalFired(std::string_view signal);

  // Returns `true` if the given signal has been fired within the specified
  // time `window`, or `false` otherwise.
  bool WasSignalFiredWithin(std::string_view signal, base::TimeDelta window);

 private:
  // Records the given `signal` in `pref_service`, using the current time as the
  // signal event time. Returns `true` if the `signal` was properly handled.
  bool RecordSignalToPref(std::string_view signal, PrefService* pref_service);

  // Weak pointer to the profile pref service.
  raw_ptr<PrefService> profile_pref_service_;

  // Weak pointer to the local-state pref service.
  raw_ptr<PrefService> local_pref_service_;

  // Validates `TipsManager` is used on the same sequence it's created on.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TIPS_MANAGER_TIPS_MANAGER_H_
