// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/sticky_activation_manager.h"

#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

TEST(StickyActivationManagerTest, ShouldActivate) {
  TestingPrefServiceSimple local_state;
  StickyActivationManager::RegisterPrefs(*local_state.registry());

  local_state.SetString(prefs::kVariationsStickyStudies, "Foo/Bar/Foo2/Bar2");
  StickyActivationManager manager(&local_state,
                                  /*sticky_activation_enabled=*/true);

  EXPECT_TRUE(manager.ShouldActivate("Foo", "Bar"));
  EXPECT_TRUE(manager.ShouldActivate("Foo2", "Bar2"));
  EXPECT_FALSE(manager.ShouldActivate("Foo3", "Bar3"));
}

TEST(StickyActivationManagerTest, SavePrefs) {
  TestingPrefServiceSimple local_state;
  StickyActivationManager::RegisterPrefs(*local_state.registry());

  local_state.SetString(prefs::kVariationsStickyStudies, "Foo/Bar/Foo2/Bar2");
  StickyActivationManager manager(&local_state,
                                  /*sticky_activation_enabled=*/true);

  auto* foo_far_trial = base::FieldTrialList::CreateFieldTrial("Foo", "Far");
  auto* bla_bla_trial = base::FieldTrialList::CreateFieldTrial("Bla", "Bla");

  EXPECT_FALSE(manager.ShouldActivate("Foo", "Far"));
  EXPECT_TRUE(manager.ShouldActivate("Foo2", "Bar2"));
  EXPECT_FALSE(manager.ShouldActivate("Foo3", "Bar3"));
  manager.StartMonitoring();
  // Only Foo2/Bar2 should be written, as the other ones did not match.
  EXPECT_EQ("Foo2/Bar2",
            local_state.GetString(prefs::kVariationsStickyStudies));

  // Activate both "Foo" and "Bla" trials. Only "Foo" should be marked since
  // "Bla" hasn't been queried using ShouldActivate() and thus isn't sticky.
  foo_far_trial->group_name();
  bla_bla_trial->group_name();
  // The pref should have been updated with Foo added and Foo2 still there.
  EXPECT_EQ("Foo/Far/Foo2/Bar2",
            local_state.GetString(prefs::kVariationsStickyStudies));
}

}  // namespace
}  // namespace variations
