// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ALOUD_TRAVERSAL_UTILS_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ALOUD_TRAVERSAL_UTILS_H_

#include <string>

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_position.h"

// Utilities for traversing the accessibility tree for Read Aloud.

// Returns the index of the next sentence of the given text, such that the
// next sentence is equivalent to text.substr(0, <returned_index>).
int GetNextSentence(const std::u16string& text, bool is_pdf);

// Returns the index of the next word of the given text, such that the
// next word is equivalent to text.substr(0, <returned_index>).
int GetNextWord(const std::u16string& text);

// Returns true if both positions are non-null and equal.
bool ArePositionsEqual(const ui::AXNodePosition::AXPositionInstance& position,
                       const ui::AXNodePosition::AXPositionInstance& other);

// Returns the correct anchor node from an AXPositionInstance that should be
// used by Read Aloud. AXPosition can sometimes return leaf nodes that don't
// actually correspond to the AXNodes we're using in Reading Mode, so we need
// to get a parent node from the AXPosition's returned anchor.
ui::AXNode* GetAnchorNode(
    const ui::AXNodePosition::AXPositionInstance& position);

// Uses the given AXNodePosition to return the next node that should be spoken
// by Read Aloud.
ui::AXNode* GetNextNodeFromPosition(
    const ui::AXNodePosition::AXPositionInstance& ax_position);

// Returns if the given character can be considered opening puncutation.
// This is used to ensure we're not reading out opening punctuation
// as a separate segment.
bool IsOpeningPunctuation(char& c);

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ALOUD_TRAVERSAL_UTILS_H_
