// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_features.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"

namespace tabs {

namespace {

// This is the generic entry point for test code to stub out TabFeature
// functionality. It is called by production code, but only used by tests.
TabFeatures::TabFeaturesFactory& GetFactory() {
  static base::NoDestructor<TabFeatures::TabFeaturesFactory> factory;
  return *factory;
}

}  // namespace

// static
std::unique_ptr<TabFeatures> TabFeatures::CreateTabFeatures(TabModel* tab) {
  if (GetFactory()) {
    return GetFactory().Run(tab);
  }
  // Constructor is protected.
  return base::WrapUnique(new TabFeatures(tab));
}

TabFeatures::~TabFeatures() = default;

// static
void TabFeatures::ReplaceTabFeaturesForTesting(TabFeaturesFactory factory) {
  TabFeatures::TabFeaturesFactory& f = GetFactory();
  f = std::move(factory);
}

TabFeatures::TabFeatures(TabModel* tab) {
  // Features that are only enabled for normal browser windows.
  if (tab->owning_model()->delegate()->IsNormalWindow()) {
    lens_overlay_controller_ = CreateLensController(tab);
  }
}

std::unique_ptr<LensOverlayController> TabFeatures::CreateLensController(
    TabModel* tab) {
  return std::make_unique<LensOverlayController>(tab);
}

}  // namespace tabs
