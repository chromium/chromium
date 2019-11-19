// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_accessibility_helper_instance.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"

namespace arc {

FakeAccessibilityHelperInstance::FakeAccessibilityHelperInstance() = default;
FakeAccessibilityHelperInstance::~FakeAccessibilityHelperInstance() = default;

void FakeAccessibilityHelperInstance::Init(
    mojom::AccessibilityHelperHostPtr host_ptr,
    InitCallback callback) {
  std::move(callback).Run();
}

void FakeAccessibilityHelperInstance::SetFilter(
    mojom::AccessibilityFilterType filter_type) {
  filter_type_ = filter_type;
}

void FakeAccessibilityHelperInstance::PerformAction(
    mojom::AccessibilityActionDataPtr action_data_ptr,
    PerformActionCallback callback) {}

void FakeAccessibilityHelperInstance::
    SetNativeChromeVoxArcSupportForFocusedWindow(
        bool enabled,
        SetNativeChromeVoxArcSupportForFocusedWindowCallback callback) {}

void FakeAccessibilityHelperInstance::SetExploreByTouchEnabled(bool enabled) {
  explore_by_touch_enabled_ = enabled;
}

void FakeAccessibilityHelperInstance::RefreshWithExtraData(
    mojom::AccessibilityActionDataPtr action_data_ptr,
    RefreshWithExtraDataCallback callback) {}

void FakeAccessibilityHelperInstance::SetCaptionStyle(
    mojom::CaptionStylePtr style_ptr) {}
}  // namespace arc
