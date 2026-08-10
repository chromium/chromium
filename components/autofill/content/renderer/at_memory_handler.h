// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_AT_MEMORY_HANDLER_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_AT_MEMORY_HANDLER_H_

#include <optional>

#include "base/containers/circular_deque.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/unique_ids.h"

namespace blink {
class WebElement;
}

namespace autofill {

// Handles AtMemory-related interactions on the renderer side.
//
// AtMemory needs to maintain special state because AtMemory
// - inserts text into specific locations in a field, rather than overwriting
//   the entire value, and
// - has high unmasking latency, so the focus or caret may have moved by the
//   time AtMemory fills an actual value into a field.
class AtMemoryHandler {
 public:
  struct AskForValuesToFillInfo {
    FieldRendererId field_id{};
    bool caused_by_trigger_string = false;
    size_t value_hash = 0;
  };

  AtMemoryHandler();
  AtMemoryHandler(const AtMemoryHandler&) = delete;
  AtMemoryHandler& operator=(const AtMemoryHandler&) = delete;
  ~AtMemoryHandler();

  // Finds the metadata for the last AtMemory-related AskForValuesToFill() on
  // `element`. If `pop` is true, removes the entry found.
  std::optional<AskForValuesToFillInfo> FindAskForValuesToFill(
      const blink::WebElement& element,
      bool pop);

  // Stores metadata for an AskForValuesToFill() on `element` if
  // `trigger_source` is related to AtMemory.
  void MaybeUpdateAskForValuesToFill(
      const blink::WebElement& element,
      AutofillSuggestionTriggerSource trigger_source);

 private:
  base::circular_deque<AskForValuesToFillInfo>
      last_at_memory_ask_for_values_to_fills_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_AT_MEMORY_HANDLER_H_
