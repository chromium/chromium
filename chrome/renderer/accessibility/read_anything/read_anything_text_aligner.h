// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_TEXT_ALIGNER_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_TEXT_ALIGNER_H_

#include <vector>

#include "ui/accessibility/ax_node_id_forward.h"

// Maps rendered Readability text blocks to the AXNodes their text came from.
class ReadAnythingTextAligner {
 public:
  // Represents a segment of text within an AXNode. A distilled Readability
  // block may be mapped to multiple AXNodes (e.g., a paragraph with a link
  // inside); each part of that mapping is a MappingSegment.
  struct MappingSegment {
    ui::AXNodeID id;
    // The 0-based start and end character offsets within the *Readability
    // block's* text content that correspond to this AXNode.
    int start;
    int end;
    // The 0-based character offset within the *AXNode's* own text content where
    // this segment begins.
    int ax_node_offset;
  };

  // The mapped segments for each rendered block, in the same order as the
  // blocks. A block that couldn't be matched has an empty entry.
  using TextToAXMap = std::vector<std::vector<MappingSegment>>;
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_TEXT_ALIGNER_H_
