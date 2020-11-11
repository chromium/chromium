// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_id_collection.h"

#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "components/variations/active_field_trials.h"

namespace variations {

VariationsIdCollection::VariationsIdCollection(
    IDCollectionKey collection_key,
    base::RepeatingCallback<void(VariationID)> new_id_callback)
    : collection_key_(collection_key) {
  base::FieldTrialList::AddObserver(this);

  base::FieldTrial::ActiveGroups initial_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&initial_groups);
  for (const base::FieldTrial::ActiveGroup& group : initial_groups) {
    OnFieldTrialGroupFinalized(group.trial_name, group.group_name);
  }

  // Delay setting |new_id_callback_| until initialization is over.
  new_id_callback_ = new_id_callback;
}

VariationsIdCollection::~VariationsIdCollection() {
  base::FieldTrialList::RemoveObserver(this);
}

void VariationsIdCollection::OnFieldTrialGroupFinalized(
    const std::string& trial_name,
    const std::string& group_name) {
  const VariationID id =
      GetGoogleVariationID(collection_key_, trial_name, group_name);
  if (id != EMPTY_ID) {
    bool modified = id_set_.insert(id).second;
    if (modified && new_id_callback_)
      new_id_callback_.Run(id);
  }
}

const std::set<VariationID>& VariationsIdCollection::GetIds() {
  return id_set_;
}

}  // namespace variations
