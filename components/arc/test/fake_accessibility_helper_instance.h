// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_ACCESSIBILITY_HELPER_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_ACCESSIBILITY_HELPER_INSTANCE_H_

#include <string>

#include "components/arc/mojom/accessibility_helper.mojom.h"

namespace arc {

class FakeAccessibilityHelperInstance
    : public mojom::AccessibilityHelperInstance {
 public:
  FakeAccessibilityHelperInstance();
  ~FakeAccessibilityHelperInstance() override;

  void Init(mojom::AccessibilityHelperHostPtr host_ptr,
            InitCallback callback) override;
  void SetFilter(mojom::AccessibilityFilterType filter_type) override;
  void PerformAction(mojom::AccessibilityActionDataPtr action_data_ptr,
                     PerformActionCallback callback) override;
  void SetNativeChromeVoxArcSupportForFocusedWindow(
      bool enabled,
      SetNativeChromeVoxArcSupportForFocusedWindowCallback callback) override;
  void SetExploreByTouchEnabled(bool enabled) override;

  mojom::AccessibilityFilterType filter_type() { return filter_type_; }
  bool explore_by_touch_enabled() { return explore_by_touch_enabled_; }
  void RefreshWithExtraData(mojom::AccessibilityActionDataPtr action_data_ptr,
                            RefreshWithExtraDataCallback callback) override;

  void SetCaptionStyle(mojom::CaptionStylePtr style_ptr) override;

 private:
  mojom::AccessibilityFilterType filter_type_ =
      mojom::AccessibilityFilterType::OFF;

  // Explore-by-touch is enabled by default in ARC++, so we default it to 'true'
  // in this test as well.
  bool explore_by_touch_enabled_ = true;

  DISALLOW_COPY_AND_ASSIGN(FakeAccessibilityHelperInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_ACCESSIBILITY_HELPER_INSTANCE_H_
