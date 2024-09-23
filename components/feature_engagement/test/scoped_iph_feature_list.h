// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TEST_SCOPED_IPH_FEATURE_LIST_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TEST_SCOPED_IPH_FEATURE_LIST_H_

#include <set>
#include <vector>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"

namespace feature_engagement::test {

// Disallows all but a set of IPH features within this object's scope,
// optionally without enabling or disabling any actual features, thus optionally
// allowing the use of existing field trial configurations.
//
// Use to test the functionality of your feature engagement features in tests.
//
// Nested ScopedIphFeatureList objects are additive; you can create an empty
// scope (using InitWithNoFeaturesAllowed()) and then create a nested scope that
// enables a single IPH.
//
// USAGE NOTE: for browser-based tests, prefer using
// `InteractiveFeaturePromoTest[T]` instead of directly using this class.
class ScopedIphFeatureList {
 public:
  ScopedIphFeatureList();
  ~ScopedIphFeatureList();
  ScopedIphFeatureList(const ScopedIphFeatureList&) = delete;
  void operator=(const ScopedIphFeatureList&) = delete;

  // Disallows all IPH features within this object's scope. Nested scopes may
  // re-enable specific features.
  void InitWithNoFeaturesAllowed();

  // Disallows all IPH features except for `allow_features`.
  //
  // Does not actually force the features in `allow_features` to be enabled.
  // Use this init method when you want to test features that are already
  // enabled in code or by a field trial and don't want to lose the existing
  // configuration.
  void InitWithExistingFeatures(
      const std::vector<base::test::FeatureRef>& allow_features);

  // Disallows all IPH features except for `allow_and_enable_features`.
  //
  //  to the set of allowed
  // features and enables those features. Use this init method when the features
  // are not already enabled in code or by a field trial, but there is a
  // client-side configuration for your IPH feature.
  //
  // If you also need to disable features, specify the second parameter.
  void InitAndEnableFeatures(
      const std::vector<base::test::FeatureRef>& allow_and_enable_features,
      const std::vector<base::test::FeatureRef>& disable_features = {});

  // Disallows all IPH features but adds `allow_and_enable_features` to the set
  // of allowed features and enables those features with configuration. Use this
  // init method to create or override a field trial, to run a test with your
  // own custom configuration, or when there is no client-side configuration to
  // draw from.
  //
  // If you also need to disable features, specify the second parameter.
  void InitAndEnableFeaturesWithParameters(
      const std::vector<base::test::FeatureRefAndParams>&
          allow_and_enable_features,
      const std::vector<base::test::FeatureRef>& disable_features = {});

  // Sets `feature` up as an IPH demo using kIPHDemoMode. `feature` will always
  // be allowed to trigger, and no other features will be allowed.
  //
  // If additional features need to be enabled for a test, `additional_features`
  // may be specified with configuration; this is similar to
  // InitAndEnableFeaturesWithParameters().
  void InitForDemo(const base::Feature& feature,
                   const std::vector<base::test::FeatureRefAndParams>&
                       additional_features = {});

  // Resets the instance to a non-initialized state.
  void Reset();

 private:
  void InitCommon(const std::vector<const base::Feature*>& features);

  bool active_ = false;
  base::test::ScopedFeatureList feature_list_;
  std::set<raw_ptr<const base::Feature, SetExperimental>> added_features_;
};

}  // namespace feature_engagement::test

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TEST_SCOPED_IPH_FEATURE_LIST_H_
