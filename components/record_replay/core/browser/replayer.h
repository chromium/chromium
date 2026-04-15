// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_REPLAYER_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_REPLAYER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/record_replay/core/browser/recording.pb.h"
#include "components/record_replay/core/common/aliases.h"
#include "components/record_replay/core/common/element_id.h"

namespace record_replay {

class ElementId;
class RecordReplayDriver;
class RecordReplayManager;

// Replays a `Recording`. Replays are asynchronous because of the delay between
// actions.
//
// Owned and used by `RecordReplayManager`. It is a transient object that only
// exists during a replay session (0 or 1 per tab). It runs exclusively on the
// UI thread and uses `base::OneShotTimer` for scheduling actions.
class Replayer {
 public:
  // Initializes the replayer. To start it, call Run().
  // `on_finish` is called on completion; it may delete `this` object.
  explicit Replayer(RecordReplayManager* owner,
                    Recording recording,
                    base::OnceClosure on_finish);
  Replayer(const Replayer&) = delete;
  Replayer& operator=(const Replayer&) = delete;
  ~Replayer();

  // Asynchronously executes the steps of the recording. Calls `on_finish`
  // when no more actions are left to execute.
  void Run();

 private:
  // Callback for actions. The `bool` indicates success or failure.
  using SuccessCallback = base::OnceCallback<void(bool)>;

  int num_actions() const;
  const Recording::Action& action(int index) const;

  // Executes the remaining actions of the given Recording, starting the
  // `index`th action. A negative index exits the loop (like `break`).
  void Loop(int index);

  // Executes the `index`th action. If it fails, retries it `num_max_retries`
  // times.
  void DoAction(int index, int num_max_retries);

  void ReplayAutofillAction(Selector element_selector,
                            const Recording::Action::AutofillSpecifics& payload,
                            SuccessCallback cb);
  void ReplayClickAction(Selector element_selector, SuccessCallback cb);
  void ReplaySelectChangeAction(Selector element_selector,
                                FieldValue value,
                                SuccessCallback cb);
  void ReplayTextChangeAction(Selector element_selector,
                              FieldValue text,
                              SuccessCallback cb);

  // Retrieves all elements in all active frames that match `element_selector`.
  // If there is exactly one match, it calls `action_cb`, which then calls
  // `result_cb`.
  // If there is no unique match, it does not call `action_cb` and calls
  // `result_cb` to report failure.
  void GetUniqueMatchingElementsAndDo(
      Selector element_selector,
      base::OnceCallback<void(RecordReplayDriver&,
                              const ElementId&,
                              SuccessCallback)> action_cb,
      SuccessCallback result_cb);

  base::WeakPtr<Replayer> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  raw_ref<RecordReplayManager> owner_;
  Recording recording_;
  base::OneShotTimer timer_;
  // Called after the last action. Since it may destroy `this`, we call it
  // asynchronously.
  base::OnceClosure on_finish_;
  base::WeakPtrFactory<Replayer> weak_ptr_factory_{this};
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_REPLAYER_H_
