// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/test_support/page_action_properties.h"

#include "base/containers/fixed_flat_map.h"

namespace page_actions {

namespace {

constexpr auto kTestPageActionControllerProperties =
    base::MakeFixedFlatMap<actions::ActionId, PageActionControllerProperties>({
        {
            0,
            {
                .histogram_name = "TestAction0",
                .is_ephemeral = true,
                .type = PageActionIconType::kLensOverlay,
            },
        },
        {
            1,
            {
                .histogram_name = "TestAction1",
                .is_ephemeral = false,
                .type = PageActionIconType::kLensOverlay,
            },
        },
        {
            2,
            {
                .histogram_name = "TestAction2",
                .is_ephemeral = true,
                .type = PageActionIconType::kLensOverlay,
            },
        },
        {
            3,
            {
                .histogram_name = "TestAction3",
                .is_ephemeral = true,
                .type = PageActionIconType::kLensOverlay,
            },
        },
        {
            4,
            {
                .histogram_name = "TestAction4",
                .is_ephemeral = true,
                .type = PageActionIconType::kLensOverlay,
            },
        },
        {
            5,
            {
                .histogram_name = "TestAction5",
                .is_ephemeral = true,
                .type = PageActionIconType::kLensOverlay,
            },
        },
    });

}  // namespace

const PageActionControllerPropertiesMap&
GetPageActionControllerTestProperties() {
  return kTestPageActionControllerProperties;
}

}  // namespace page_actions
