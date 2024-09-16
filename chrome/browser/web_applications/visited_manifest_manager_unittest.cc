// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/visited_manifest_manager.h"

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app {
namespace {

GURL GetExampleUrl(int i) {
  return GURL(
      base::StrCat({"https://www.example", base::ToString(i), ".com/"}));
}

blink::mojom::ManifestPtr CreateManifestWithScope(const GURL& scope) {
  blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
  manifest->start_url = GURL(scope.GetWithoutFilename().spec() + "index.html");
  manifest->id = manifest->start_url;
  manifest->scope = scope;
  manifest->has_valid_specified_start_url = true;
  return manifest;
}

blink::mojom::ManifestPtr CreatManifestWithScopeNoStartUrl(const GURL& scope) {
  blink::mojom::ManifestPtr manifest = CreateManifestWithScope(scope);
  manifest->has_valid_specified_start_url = false;
  return manifest;
}

blink::mojom::ManifestPtr CreateExampleManifest(int i) {
  return CreateManifestWithScope(GetExampleUrl(i));
}

class VisitedManagerEnabledTest : public testing::Test {
 private:
  base::test::ScopedFeatureList enable_feature{
      kBlockMlPromotionInNestedPagesNoManifest};
};

TEST_F(VisitedManagerEnabledTest, ControlledChecks) {
  VisitedManifestManager manager;

  manager.OnManifestSeen(
      *CreateManifestWithScope(GURL("https://www.example0.com/pre/")));
  manager.OnManifestSeen(
      *CreateManifestWithScope(GURL("https://www.example1.com/seen/")));
  manager.OnManifestSeen(
      *CreateManifestWithScope(GURL("https://www.example2.com/seeo/")));

  EXPECT_TRUE(manager.IsUrlControlledBySeenManifest(
      GURL("https://www.example1.com/seen/")));
  EXPECT_TRUE(manager.IsUrlControlledBySeenManifest(
      GURL("https://www.example1.com/seen/nest")));
  EXPECT_TRUE(manager.IsUrlControlledBySeenManifest(
      GURL("https://www.example1.com/seen/nested/")));
  // Before 'seen/' in index ordering, and matches path on other origin
  EXPECT_FALSE(manager.IsUrlControlledBySeenManifest(
      GURL("https://www.example1.com/pre/")));
  EXPECT_FALSE(manager.IsUrlControlledBySeenManifest(
      GURL("https://www.example1.com/seen")));
  // After 'seen/' in index ordering, and matches path on other origin
  EXPECT_FALSE(manager.IsUrlControlledBySeenManifest(
      GURL("https://www.example1.com/seeo/")));
}

TEST_F(VisitedManagerEnabledTest, ClearDateRange) {
  base::SimpleTestClock clock;
  VisitedManifestManager manager(&clock);

  base::Time start = base::Time::Now();
  clock.SetNow(start);
  manager.OnManifestSeen(
      *CreateManifestWithScope(GURL("https://www.example0.com/")));

  clock.Advance(base::Minutes(10));
  manager.OnManifestSeen(
      *CreateManifestWithScope(GURL("https://www.example1.com/")));

  clock.Advance(base::Minutes(10));
  manager.OnManifestSeen(
      *CreateManifestWithScope(GURL("https://www.example2.com/")));

  clock.Advance(base::Minutes(10));
  manager.OnManifestSeen(
      *CreateManifestWithScope(GURL("https://www.example3.com/")));

  // This should clear examples 1 and 2.
  manager.ClearSeenScopes(start + base::Minutes(10), start + base::Minutes(20));

  base::Time check_time = clock.Now();

  EXPECT_TRUE(
      manager.IsUrlControlledBySeenManifest(GURL("https://www.example0.com/")));
  EXPECT_FALSE(
      manager.IsUrlControlledBySeenManifest(GURL("https://www.example1.com/")));
  EXPECT_FALSE(
      manager.IsUrlControlledBySeenManifest(GURL("https://www.example2.com/")));
  EXPECT_TRUE(
      manager.IsUrlControlledBySeenManifest(GURL("https://www.example3.com/")));

  // Checking the entries also updates the time, so clearing at the check_time
  // should clear those entries too.
  manager.ClearSeenScopes(check_time, check_time);

  EXPECT_FALSE(
      manager.IsUrlControlledBySeenManifest(GURL("https://www.example0.com/")));
  EXPECT_FALSE(
      manager.IsUrlControlledBySeenManifest(GURL("https://www.example3.com/")));
}

TEST_F(VisitedManagerEnabledTest, TestLru) {
  VisitedManifestManager manager;
  // Fill up the cache. Eviction should start from 0, as that is the first one
  // that was added.
  for (int i = 0; i < VisitedManifestManager::kMaxScopesToSave; ++i) {
    manager.OnManifestSeen(*CreateExampleManifest(i));
  }
  // Check they are all present, in order again which should keep the same
  // ordering.
  for (int i = 0; i < VisitedManifestManager::kMaxScopesToSave; ++i) {
    EXPECT_TRUE(manager.IsUrlControlledBySeenManifest(GetExampleUrl(i)));
  }
  // Test that is '0' was evicted after adding one more.
  manager.OnManifestSeen(
      *CreateExampleManifest(VisitedManifestManager::kMaxScopesToSave));
  EXPECT_FALSE(manager.IsUrlControlledBySeenManifest(GetExampleUrl(0)));

  // Test `OnManifestSeen` and `IsUrlControlledBySeenManifest` both move the
  // entry to the top of the queue. '1' and '2' will not be deleted, instead '3'
  // and '4' will when adding two more manifests.
  manager.OnManifestSeen(*CreateExampleManifest(1));
  EXPECT_TRUE(manager.IsUrlControlledBySeenManifest(GetExampleUrl(2)));
  manager.OnManifestSeen(
      *CreateExampleManifest(VisitedManifestManager::kMaxScopesToSave + 1));
  manager.OnManifestSeen(
      *CreateExampleManifest(VisitedManifestManager::kMaxScopesToSave + 2));
  EXPECT_FALSE(manager.IsUrlControlledBySeenManifest(GetExampleUrl(3)));
  EXPECT_FALSE(manager.IsUrlControlledBySeenManifest(GetExampleUrl(4)));

  // Final check that all is correct.
  for (int i = 0; i < VisitedManifestManager::kMaxScopesToSave + 3; ++i) {
    bool is_controlled =
        manager.IsUrlControlledBySeenManifest(GetExampleUrl(i));
    if (i == 0 || i == 3 || i == 4) {
      EXPECT_FALSE(is_controlled) << i;
    } else {
      EXPECT_TRUE(is_controlled) << i;
    }
  }
}

TEST_F(VisitedManagerEnabledTest, TestInvalidManifests) {
  VisitedManifestManager manager;
  manager.OnManifestSeen(
      *CreatManifestWithScopeNoStartUrl(GURL("https://www.example0.com/")));
  EXPECT_FALSE(
      manager.IsUrlControlledBySeenManifest(GURL("https://www.example0.com/")));
}

TEST(VisitedManagerDisabledTest, DisabledFeature) {
  // Ensure that we can disable the feature and have things not crash, and
  // always returns 'false'.
  base::test::ScopedFeatureList disable;
  disable.InitAndDisableFeature(kBlockMlPromotionInNestedPagesNoManifest);
  base::SimpleTestClock clock;
  VisitedManifestManager manager(&clock);

  base::Time start = base::Time::Now();
  clock.SetNow(start);
  manager.OnManifestSeen(
      *CreateManifestWithScope(GURL("https://www.example0.com/")));
  manager.OnManifestSeen(
      *CreateManifestWithScope(GURL("https://www.example1.com/")));

  base::Time check_time = clock.Now();

  EXPECT_FALSE(
      manager.IsUrlControlledBySeenManifest(GURL("https://www.example0.com/")));
  EXPECT_FALSE(
      manager.IsUrlControlledBySeenManifest(GURL("https://www.example1.com/")));

  manager.ClearSeenScopes(start, check_time);
}

}  // namespace
}  // namespace web_app
