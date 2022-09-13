// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_UI_ACTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_UI_ACTION_DELEGATE_H_

#include "base/callback.h"
#include "base/timer/timer.h"
#include "components/autofill_assistant/browser/android/assistant_ui_action_delegate.h"
#include "components/autofill_assistant/browser/public/external_action.pb.h"
#include "components/autofill_assistant/browser/public/external_action_delegate.h"
#include "components/autofill_assistant/browser/public/headless_script_controller.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {
class AssistantUiActionDelegate : public ExternalActionDelegate {
 public:
  AssistantUiActionDelegate();
  ~AssistantUiActionDelegate() override;

  void OnActionRequested(
      const external::Action& action_info,
      bool is_interrupt,
      base::OnceCallback<void(ExternalActionDelegate::DomUpdateCallback)>
          start_dom_checks_callback,
      base::OnceCallback<void(const external::Result& result)>
          end_action_callback) override;
  void OnInterruptStarted() override;
  void OnInterruptFinished() override;

 private:
  void OnDomUpdateReceived(const external::ElementConditionsUpdate& update);
  void OnTimeout();
  void EndAction(bool success,
                 absl::optional<assistant_ui::AssistantUiActionResult>
                     action_result = absl::nullopt);

  std::unique_ptr<base::OneShotTimer> timeout_timer_;

  // The callback that terminates the current action.
  base::OnceCallback<void(const external::Result& result)> end_action_callback_;

  base::WeakPtrFactory<AssistantUiActionDelegate> weak_ptr_factory_{this};
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_UI_ACTION_DELEGATE_H_
