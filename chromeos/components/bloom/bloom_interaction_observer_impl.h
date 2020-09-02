// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_BLOOM_BLOOM_INTERACTION_OBSERVER_IMPL_H_
#define CHROMEOS_COMPONENTS_BLOOM_BLOOM_INTERACTION_OBSERVER_IMPL_H_

#include "chromeos/components/bloom/bloom_interaction_observer.h"

namespace ash {
class AssistantInteractionController;
}  // namespace ash

namespace chromeos {
namespace bloom {

// Main observer of the Bloom interaction events.
// Will trigger the Assistant service so it can open/close the UI and display
// all bloom responses.
class BloomInteractionObserverImpl : public BloomInteractionObserver {
 public:
  BloomInteractionObserverImpl();
  ~BloomInteractionObserverImpl() override;

  // BloomInteractionObserver implementation:
  void OnInteractionStarted() override;
  void OnShowUI() override;
  void OnShowResult(const std::string& html) override;
  void OnInteractionFinished(BloomInteractionResolution resolution) override;

 private:
  // Return the current |AssistantInteractionController|. Can be nullptr
  // (usually if the system is in the process of shutting down/logging out).
  ash::AssistantInteractionController* assistant_interaction_controller();
};

}  // namespace bloom
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_BLOOM_BLOOM_INTERACTION_OBSERVER_IMPL_H_
