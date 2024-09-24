// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TIPS_MANAGER_TIPS_MANAGER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TIPS_MANAGER_TIPS_MANAGER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

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

  ~TipsManager() override = default;

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

  // Notifies the TipsManager about an observed signal event.
  // This triggers:
  //
  // 1. Internal state updates for relevant Tip(s).
  // 2. Recording of the signal in UMA histograms.
  // 3. Persistence of the signal data in Prefs for future use.
  void NotifySignal(const std::string& signal);

 private:
  // Weak pointer to the pref service.
  raw_ptr<PrefService> pref_service_;

  // Weak pointer to the local-state pref service.
  raw_ptr<PrefService> local_pref_service_;

  // Validates `TipsManager` is used on the same sequence it's created on.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TIPS_MANAGER_TIPS_MANAGER_H_
