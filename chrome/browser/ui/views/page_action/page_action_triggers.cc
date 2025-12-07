// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_triggers.h"

#include "ui/base/class_property.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(page_actions::PageActionTrigger)

namespace page_actions {

DEFINE_UI_CLASS_PROPERTY_KEY(std::underlying_type_t<PageActionTrigger>,
                             kPageActionTriggerKey,
                             kInvalidPageActionTrigger)
}  // namespace page_actions
