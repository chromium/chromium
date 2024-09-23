// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_USER_EDUCATION_METADATA_H_
#define COMPONENTS_USER_EDUCATION_COMMON_USER_EDUCATION_METADATA_H_

#include <initializer_list>
#include <string>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"

namespace user_education {

// Provides metadata about a user education experience (IPH, Tutorial, "New"
// Badge, etc.).
//
// Metadata will be shown and used on the tester page
// (chrome://internals/user-education), and also provides information as to when
// the journey was added to Chrome and by whom.
struct Metadata {
  // The platform the experience can be shown on.
  //
  // These are a subset of variations::Study::Platform.
  //
  // TODO(dfried): figure out how to unify a single list of platforms for all
  // use cases; enums like this are scattered all over the codebase.
  enum class Platforms {
    kWindows = 0,
    kMac = 1,
    kLinux = 2,
    kChromeOSAsh = 3,
    kChromeOSLacros = 9,
  };

  // All desktop platforms.
  static constexpr std::initializer_list<Platforms> kAllDesktopPlatforms{
      Platforms::kWindows, Platforms::kMac, Platforms::kLinux,
      Platforms::kChromeOSAsh, Platforms::kChromeOSLacros};

  using FeatureSet =
      base::flat_set<raw_ptr<const base::Feature, CtnExperimental>>;
  using PlatformSet = base::flat_set<Platforms>;

  Metadata(int launch_milestone,
           std::string owners,
           std::string additional_description,
           FeatureSet required_features = {},
           PlatformSet platforms = kAllDesktopPlatforms);
  Metadata();
  Metadata(Metadata&&) noexcept;
  Metadata& operator=(Metadata&&) noexcept;
  ~Metadata();

  // The integer part of the launch milestone. For example, 118.
  int launch_milestone = 0;

  // The email, ldap, group name, team name, etc. of the owner(s) of the
  // experience. This is a display-only field on an internal page, so the format
  // is up to the implementing team, but it is also metadata to track each
  // experience's lifecycle so be sure to specify it.
  std::string owners;

  // Additional description of the user education experience. This could include
  // clarification of how and when the experience is shown to the user, the goal
  // of presenting the experience, etc.
  //
  // This is a display-only field on an internal page, so the format is up to
  // the implementing team, but a good description will help other people
  // understand how and why the experience was implemented as well as when to
  // expect it to appear.
  std::string additional_description;

  // The set of additional features that must be enabled in order for this
  // experience to be displayed. Does not include the `base::Feature` for the
  // experience itself.
  FeatureSet required_features;

  // The set of platforms the experience can be displayed on.
  PlatformSet platforms = kAllDesktopPlatforms;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_USER_EDUCATION_METADATA_H_
