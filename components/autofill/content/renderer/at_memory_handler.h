// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_AT_MEMORY_HANDLER_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_AT_MEMORY_HANDLER_H_

#include <optional>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/timing.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {
class FieldDataManager;
}

namespace blink {
class WebElement;
class WebFormControlElement;
class WebKeyboardEvent;
struct RendererPreferences;
}  // namespace blink

namespace ukm {
class MojoUkmRecorder;
class UkmRecorder;
}  // namespace ukm

namespace autofill {

class AutofillAgent;
class SynchronousFormCache;

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

  explicit AtMemoryHandler(AutofillAgent* agent);
  AtMemoryHandler(const AtMemoryHandler&) = delete;
  AtMemoryHandler& operator=(const AtMemoryHandler&) = delete;
  ~AtMemoryHandler();

  // Handles value changes in contenteditable elements. Returns true if AtMemory
  // handled the event (i.e. triggered suggestions).
  bool ContentEditableDidChange(const blink::WebElement& element);

  // Handles value changes in text fields. Returns true if AtMemory handled the
  // event (i.e. triggered suggestions).
  bool OnTextFieldValueChanged(const blink::WebFormControlElement& element,
                               const SynchronousFormCache& form_cache);

  // Handles key down events for AtMemory (e.g. keyboard shortcuts). Returns
  // true if the event was handled (i.e. default action should be prevented).
  bool DidReceiveKeyDown(const blink::WebElement& element,
                         const blink::WebKeyboardEvent& event);

  // Tries to fill `value` into `element` at the location where AtMemory was
  // last triggered on `element`.
  void ReplaceSelectionForAtMemory(blink::WebElement& element,
                                   const std::u16string& value);

  // Stores metadata for an AskForValuesToFill() on `element` if
  // `trigger_source` is related to AtMemory.
  void MaybeUpdateAskForValuesToFill(
      const blink::WebElement& element,
      AutofillSuggestionTriggerSource trigger_source);

 private:
  const blink::RendererPreferences* GetRendererPreferences() const;

  const std::string& GetTriggerString() const;

  bool ShouldTriggerAtMemorySearch(const blink::WebElement& element) const;

  // Finds the metadata for the last AtMemory-related AskForValuesToFill() on
  // `element`. If `pop` is true, removes the entry found.
  std::optional<AskForValuesToFillInfo> FindAskForValuesToFill(
      const blink::WebElement& element,
      bool pop);

  // Records a UKM event if the user pressed "@" twice in quick succession.
  void MaybeRecordAtAt(const blink::WebElement& element,
                       const blink::WebKeyboardEvent& event,
                       const FieldDataManager& field_data_manager,
                       const CallTimerState& timer_state,
                       form_util::ButtonTitlesCache* button_titles_cache);

  ukm::UkmRecorder* GetUkmRecorder();

  const raw_ref<AutofillAgent> agent_;
  base::circular_deque<AskForValuesToFillInfo>
      last_at_memory_ask_for_values_to_fills_;

  std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder_;

  struct {
    base::TimeTicks time;
    FieldRendererId field;
  } last_at_key_press_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_AT_MEMORY_HANDLER_H_
