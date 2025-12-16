// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_KEY_DISPATCHER_H_
#define CHROME_RENDERER_ACTOR_KEY_DISPATCHER_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/renderer/actor/journal.h"
#include "chrome/renderer/actor/tool_base.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"

namespace blink {

class WebWidget;

}  // namespace blink

namespace actor {

class TypeTool;

// Handles the sequence of KeyParams with delays in between each, and an initial
// delay.
//
// If cancel is called and a key press has started, this will synchronously
// inject the key up event to ensure the page is left in a consistent state.
//
// The `on_complete` caller may safely destroy this ClickDispatcher.
class KeyDispatcher {
 public:
  // Structure to hold all necessary parameters for generating keyboard events
  // for a single character or key press.
  struct KeyParams {
    KeyParams();
    ~KeyParams();
    KeyParams(const KeyParams& other);
    int windows_key_code;
    int native_key_code;
    // Physical key identifier string
    std::string dom_code;
    // Character produced, considering modifiers
    std::string dom_key;
    int modifiers = blink::WebInputEvent::kNoModifiers;
    // Text character for kChar event
    char16_t text = u'\0';
    // Text without modifiers
    char16_t unmodified_text = u'\0';
  };

  // Starts the sequence of key events upon creation, after an initial delay.
  //
  // The passed in `type_tool` must own this instance.
  //
  // `on_complete` is always called asynchronously, but isn't invoked if
  // Cancel() is invoked first.
  KeyDispatcher(std::vector<KeyParams> key_sequence,
                mojom::TypeActionPtr action,
                const ResolvedTarget& resolved_target,
                const TypeTool& type_tool,
                ToolBase::ToolFinishedCallback on_complete,
                TaskId task_id,
                Journal& journal);
  ~KeyDispatcher();

  // Cancels future dispatching, with the exception that if a key is in a down
  // state, key up will be dispatched.
  void Cancel();

 private:
  // Proceed to the next key event in the sequence.
  void ContinueIncrementalTyping();

  // Asynchronously calls `on_complete_` with `result`. Does nothing if already
  // called, or if Cancel() has been called.
  void Finish(mojom::ActionResultPtr result);

  blink::WebInputEventResult CreateAndDispatchKeyEvent(
      blink::WebWidget& widget,
      blink::WebInputEvent::Type type,
      KeyParams key_params);
  KeyParams GetEnterKeyParams() const;

  // The sequence of keys to type.
  const std::vector<KeyParams> key_sequence_;

  // Information on how to type `key_sequence_`.
  const mojom::TypeActionPtr action_;

  // Target of typing action, owned by the TypeTool, which owns this dispatcher.
  raw_ref<const ResolvedTarget> resolved_target_;

  // Back reference to the TypeTool which owns this dispatcher.
  raw_ref<const TypeTool> type_tool_;

  // Callback to invoke asynchronously after the sequence of events has
  // completed, or upon Cancel().
  ToolBase::ToolFinishedCallback on_complete_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_ =
      base::SequencedTaskRunner::GetCurrentDefault();

  // true if a key is currently held down (and needs to be lifted), or false
  // otherwise.
  bool is_key_down_ = false;

  // Index into `key_sequence_` of the current key to press.
  size_t current_key_ = 0;

  TaskId task_id_;
  base::raw_ref<Journal> journal_;

  base::WeakPtrFactory<KeyDispatcher> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_KEY_DISPATCHER_H_
