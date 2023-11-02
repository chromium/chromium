// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FLAGS_UI_FLAGS_TEST_HELPERS_H_
#define COMPONENTS_FLAGS_UI_FLAGS_TEST_HELPERS_H_

#include "base/containers/span.h"
#include "components/flags_ui/feature_entry.h"

namespace flags_ui {

namespace testing {

// Ensures that all flags in |entries| has associated metadata. |count| is the
// number of flags in |entries|.
void EnsureEveryFlagHasMetadata(
    const base::span<const flags_ui::FeatureEntry>& entries);

// Ensures that all flags marked as never expiring in flag-metadata.json is
// listed in flag-never-expire-list.json.
void EnsureOnlyPermittedFlagsNeverExpire();

// Ensures that every flag has an owner.
void EnsureEveryFlagHasNonEmptyOwners();

// Ensures that owners conform to rules in flag-metadata.json.
void EnsureOwnersLookValid();

// Ensures that flags are listed in alphabetical order in flag-metadata.json and
// flag-never-expire-list.json.
void EnsureFlagsAreListedInAlphabeticalOrder();

// Ensures that unexpire flags are present for the most recent two milestones,
// in accordance with the policy in //docs/flag_expiry.md.
void EnsureRecentUnexpireFlagsArePresent(
    const base::span<const FeatureEntry>& entries,
    int current_milestone);

}  // namespace testing

}  // namespace flags_ui

#endif  // COMPONENTS_FLAGS_UI_FLAGS_TEST_HELPERS_H_
