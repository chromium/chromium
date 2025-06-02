// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_features.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_group_desktop.h"

namespace {

// This is the generic entry point for test code to stub out TabGroupFeature
// functionality. It is called by production code, but only used by tests.
TabGroupFeatures::TabGroupFeaturesFactory& GetFactory() {
  static base::NoDestructor<TabGroupFeatures::TabGroupFeaturesFactory> factory;
  return *factory;
}

}  // namespace

// static
std::unique_ptr<TabGroupFeatures> TabGroupFeatures::CreateTabGroupFeatures() {
  if (GetFactory()) {
    return GetFactory().Run();
  }
  // Constructor is protected.
  return base::WrapUnique(new TabGroupFeatures());
}

// static
void TabGroupFeatures::ReplaceTabGroupFeaturesForTesting(
    TabGroupFeaturesFactory factory) {
  TabGroupFeatures::TabGroupFeaturesFactory& f = GetFactory();
  f = std::move(factory);
}

TabGroupFeatures::~TabGroupFeatures() = default;

void TabGroupFeatures::Init(TabGroupDesktop& group, Profile* profile) {
  CHECK(!initialized_);
  initialized_ = true;
}

TabGroupFeatures::TabGroupFeatures() = default;
