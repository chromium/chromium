// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_TEST_UTILS_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_TEST_UTILS_H_

#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_position.h"

namespace test {

void SetUpdateTreeID(ui::AXTreeUpdate* update, ui::AXTreeID tree_id);
ui::AXTreeUpdate& CreateInitialUpdate();

}  // namespace test

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_TEST_UTILS_H_
