// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/actor/actor_autofill_manager.h"

#include "base/feature_list.h"
#include "components/autofill/core/browser/actor/actor_key_metrics_recorder.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/common/autofill_debug_features.h"

namespace autofill {

ActorAutofillManager::ActorAutofillManager(AutofillClient* client)
    : key_metrics_recorder_(client) {}

ActorAutofillManager::~ActorAutofillManager() = default;

bool ActorAutofillManager::IsTabInActorMode() const {
  if (base::FeatureList::IsEnabled(features::debug::kAutofillForceActorMode)) {
    return true;
  }
  return active_actor_task_.has_value();
}

}  // namespace autofill
