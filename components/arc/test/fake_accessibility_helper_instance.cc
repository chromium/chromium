// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_accessibility_helper_instance.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"

namespace arc {

FakeAccessibilityHelperInstance::FakeAccessibilityHelperInstance() = default;
FakeAccessibilityHelperInstance::~FakeAccessibilityHelperInstance() = default;

void FakeAccessibilityHelperInstance::Init(
    mojo::PendingRemote<mojom::AccessibilityHelperHost> host_remote,
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
        SetNativeChromeVoxArcSupportForFocusedWindowCallback callback) {
  std::move(callback).Run(true);
}

void FakeAccessibilityHelperInstance::SetExploreByTouchEnabled(bool enabled) {
  explore_by_touch_enabled_ = enabled;
}

void FakeAccessibilityHelperInstance::RefreshWithExtraData(
    mojom::AccessibilityActionDataPtr action_data_ptr,
    RefreshWithExtraDataCallback callback) {}

void FakeAccessibilityHelperInstance::SetCaptionStyle(
    mojom::CaptionStylePtr style_ptr) {}

void FakeAccessibilityHelperInstance::RequestSendAccessibilityTree(
    mojom::AccessibilityWindowKeyPtr window_ptr) {
  last_requested_tree_window_key_ = std::move(window_ptr);
}

}  // namespace arc
