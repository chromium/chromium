// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_triggers.h"

#include "ui/base/class_property.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(page_actions::PageActionTrigger)

namespace {
constexpr std::underlying_type_t<page_actions::PageActionTrigger>
    kInvalidPageActionTriggerSource = -1;
}

namespace page_actions {

DEFINE_UI_CLASS_PROPERTY_KEY(std::underlying_type_t<PageActionTrigger>,
                             kPageActionTriggerKey,
                             kInvalidPageActionTriggerSource)
}  // namespace page_actions
