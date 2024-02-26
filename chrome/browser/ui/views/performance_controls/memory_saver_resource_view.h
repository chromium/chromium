// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_MEMORY_SAVER_RESOURCE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_MEMORY_SAVER_RESOURCE_VIEW_H_

#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"

// Represents a visualization of the memory savings while this tab was
// previously inactive. This will show the total memory savings and a
// visualization to contextualize those savings.
class MemorySaverResourceView : public views::FlexLayoutView {
 public:
  explicit MemorySaverResourceView(const int64_t memory_savings_bytes);

  MemorySaverResourceView(const MemorySaverResourceView&) = delete;
  MemorySaverResourceView& operator=(const MemorySaverResourceView&) = delete;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(
      kMemorySaverResourceViewMemorySavingsElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(
      kMemorySaverResourceViewMemoryLabelElementId);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_MEMORY_SAVER_RESOURCE_VIEW_H_
