// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/test_support/page_action_test_support.h"

#include "base/check.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/page_action/page_action_properties_provider.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view_interface.h"

namespace page_actions {

IconLabelBubbleView* GetIconLabelBubbleViewForTesting(
    PageActionViewInterface* interface_ptr,
    actions::ActionId action_id) {
  CHECK(!features::IsWebUILocationBarEnabled());
  if (!interface_ptr) {
    return nullptr;
  }
  PageActionPropertiesProvider provider;
  if (!provider.Contains(action_id)) {
    return nullptr;
  }
  const auto& properties = provider.GetProperties(action_id);
  if (IsPageActionMigrated(properties.type)) {
    return static_cast<PageActionView*>(interface_ptr);
  } else {
    return static_cast<PageActionIconView*>(interface_ptr);
  }
}

}  // namespace page_actions
