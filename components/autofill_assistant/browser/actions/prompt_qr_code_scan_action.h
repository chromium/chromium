// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_PROMPT_QR_CODE_SCAN_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_PROMPT_QR_CODE_SCAN_ACTION_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {

// An action to prompt QR Code Scanning.
class PromptQrCodeScanAction : public Action {
 public:
  explicit PromptQrCodeScanAction(ActionDelegate* delegate,
                                  const ActionProto& proto);

  PromptQrCodeScanAction(const PromptQrCodeScanAction&) = delete;
  PromptQrCodeScanAction& operator=(const PromptQrCodeScanAction&) = delete;

  ~PromptQrCodeScanAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void EndAction(const ClientStatus& status,
                 const absl::optional<ValueProto>& value);

  ProcessActionCallback callback_;
  base::WeakPtrFactory<PromptQrCodeScanAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_PROMPT_QR_CODE_SCAN_ACTION_H_
