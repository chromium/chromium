// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_H_

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {
// Class to execute an assistant script.
class ScriptExecutor : public ActionDelegate {
 public:
  // Listens to events on ScriptExecutor.
  // TODO(b/806868): Make server_payload a part of callback instead of the
  // listener.
  class Listener {
   public:
    virtual ~Listener() = default;

    // Called when new server_payload is available.
    virtual void OnServerPayloadChanged(const std::string& server_payload) = 0;
  };

  // |delegate| and |listener| should outlive this object and should not be
  // nullptr.
  ScriptExecutor(const std::string& script_path,
                 const std::string& server_payload,
                 ScriptExecutor::Listener* listener,
                 ScriptExecutorDelegate* delegate);
  ~ScriptExecutor() override;

  // What should happen after the script has run.
  enum AtEnd {
    // Continue normally.
    CONTINUE = 0,

    // Shut down Autofill Assistant.
    SHUTDOWN,

    // Reset all state and restart.
    RESTART
  };

  // Contains the result of the Run operation.
  struct Result {
    bool success = false;
    AtEnd at_end = AtEnd::CONTINUE;
  };

  using RunScriptCallback = base::OnceCallback<void(Result)>;
  void Run(RunScriptCallback callback);

  // Override ActionDelegate:
  std::unique_ptr<BatchElementChecker> CreateBatchElementChecker() override;
  void WaitForElement(const std::vector<std::string>& selectors,
                      base::OnceCallback<void(bool)> callback) override;
  void ShowStatusMessage(const std::string& message) override;
  void ClickElement(const std::vector<std::string>& selectors,
                    base::OnceCallback<void(bool)> callback) override;
  void GetPaymentInformation(
      payments::mojom::PaymentOptionsPtr payment_options,
      base::OnceCallback<void(std::unique_ptr<PaymentInformation>)> callback,
      const std::string& title) override;
  void ChooseAddress(
      base::OnceCallback<void(const std::string&)> callback) override;
  void FillAddressForm(const std::string& guid,
                       const std::vector<std::string>& selectors,
                       base::OnceCallback<void(bool)> callback) override;
  void ChooseCard(
      base::OnceCallback<void(const std::string&)> callback) override;
  void FillCardForm(std::unique_ptr<autofill::CreditCard> card,
                    const base::string16& cvc,
                    const std::vector<std::string>& selectors,
                    base::OnceCallback<void(bool)> callback) override;
  void SelectOption(const std::vector<std::string>& selectors,
                    const std::string& selected_option,
                    base::OnceCallback<void(bool)> callback) override;
  void HighlightElement(const std::vector<std::string>& selectors,
                        base::OnceCallback<void(bool)> callback) override;
  void FocusElement(const std::vector<std::string>& selectors,
                    base::OnceCallback<void(bool)> callback) override;
  void SetFieldValue(const std::vector<std::string>& selectors,
                     const std::string& value,
                     bool simulate_key_presses,
                     base::OnceCallback<void(bool)> callback) override;
  void GetOuterHtml(
      const std::vector<std::string>& selectors,
      base::OnceCallback<void(bool, const std::string&)> callback) override;
  void LoadURL(const GURL& url) override;
  void Shutdown() override;
  void Restart() override;
  ClientMemory* GetClientMemory() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  content::WebContents* GetWebContents() override;
  void StopCurrentScript(const std::string& message) override;
  void HideDetails() override;
  void ShowDetails(const DetailsProto& details) override;
  void ShowProgressBar(int progress, const std::string& message) override;
  void HideProgressBar() override;
  void ShowOverlay() override;
  void HideOverlay() override;

 private:
  void OnGetActions(bool result, const std::string& response);
  void RunCallback(bool success);
  void ProcessNextAction();
  void ProcessAction(Action* action);
  void GetNextActions();
  void OnProcessedAction(std::unique_ptr<ProcessedActionProto> action);

  std::string script_path_;
  std::string last_server_payload_;
  ScriptExecutor::Listener* const listener_;
  ScriptExecutorDelegate* delegate_;
  RunScriptCallback callback_;

  std::vector<std::unique_ptr<Action>> actions_;
  std::vector<ProcessedActionProto> processed_actions_;
  AtEnd at_end_;
  bool should_stop_script_;
  bool should_clean_contextual_ui_on_finish_;

  base::WeakPtrFactory<ScriptExecutor> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ScriptExecutor);
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_H_
