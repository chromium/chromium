// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_AT_MEMORY_HANDLER_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_AT_MEMORY_HANDLER_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/timing.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/is_required.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/blink/public/web/web_range.h"

namespace autofill {
class FieldDataManager;
}

namespace blink {
class WebElement;
class WebKeyboardEvent;
class WebNode;
struct RendererPreferences;
}  // namespace blink

namespace ukm {
class MojoUkmRecorder;
class UkmRecorder;
}  // namespace ukm

namespace autofill {

class AutofillAgent;

// Handles AtMemory-related interactions on the renderer side. It has two main
// jobs:
//
// Firstly, it observes two possible AtMemory triggers: the trigger string and
// the keyboard shortcut. Both are handled in DidReceiveKeyDown().
//
// Secondly, it maintains state between the triggering of suggestions and
// filling operations. Unlike classical Autofill, AtMemory needs such state
// because it
// - inserts text into specific locations in a field, rather than overwriting
//   the entire value, and
// - has high unmasking latency, so the focus or caret may have moved by the
//   time AtMemory fills an actual value into a field.
//
// Owned by AutofillAgent. AutofillAgent passes the relevant events to
// AtMemoryHandler.
class AtMemoryHandler {
 public:
  struct AskForValuesToFillInfo {
    FieldRendererId field_id{};
    bool caused_by_trigger_string = false;
    size_t value_hash = 0;
    blink::WebRange selection_range;
  };

  explicit AtMemoryHandler(AutofillAgent* agent);
  AtMemoryHandler(const AtMemoryHandler&) = delete;
  AtMemoryHandler& operator=(const AtMemoryHandler&) = delete;
  ~AtMemoryHandler();

  // May trigger the AtMemory suggestion if the keydown event completes
  // AtMemory's trigger string or the keyboard shortcut.
  // Returns true in the latter case to indicate that the browser must not
  // default-handle the shortcut (in particular: not bubble up the keyboard
  // shortcut to the browser process).
  bool DidReceiveKeyDown(const blink::WebElement& element,
                         const blink::WebKeyboardEvent& event);

  void FocusedElementChanged(const blink::WebElement& new_focused_element);

  void DidReceiveLeftMouseDownOrGestureTapInNode(const blink::WebNode& node);

  // Tries to fill `value` into `element` at the location where AtMemory was
  // last triggered on `element`.
  void ReplaceSelectionForAtMemory(blink::WebElement element,
                                   const std::u16string& value);

  // Stores metadata for an AskForValuesToFill() on `element` if
  // `trigger_source` is related to AtMemory.
  void MaybeUpdateAskForValuesToFill(
      const blink::WebElement& element,
      AutofillSuggestionTriggerSource trigger_source);

 private:
  const blink::RendererPreferences* GetRendererPreferences() const;

  const std::u16string& GetTriggerString() const;

  // Returns true if the trigger string occurs before the caret in `element`.
  bool HasTriggerStringNextToCaret(const blink::WebElement& element) const;

  bool DidReceiveKeyDownForAtMemoryShortcut(
      const blink::WebElement& element,
      const blink::WebKeyboardEvent& event);

  void DidReceiveKeyDownForAtMemoryTriggerString(
      const blink::WebElement& element,
      const blink::WebKeyboardEvent& event);

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

  // State for observing coherent trigger string input.
  struct {
    // The longest suffix of coherent user input that is a prefix of the trigger
    // string. These characters do not necessarily occur in the field value.
    std::u16string seen_trigger;
    // The time of the last keydown event. Only events that happen in a certain
    // timespan are considered coherent.
    base::TimeTicks last_time;
    // The target of the last keydown event.
    FieldRendererId last_element_id{};
    // The caret offset before (!) the character occurs.
    // Note that the character might not appear at all, e.g., in
    // <input type=number>.
    size_t last_offset = std::string::npos;
  } trigger_state_;

  // State for the "@@" UKM metric.
  struct {
    base::TimeTicks time;
    FieldRendererId field;
  } last_at_key_press_;

  std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder_;

  base::WeakPtrFactory<AtMemoryHandler> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_AT_MEMORY_HANDLER_H_
