// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_GET_ELEMENT_STATUS_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_GET_ELEMENT_STATUS_ACTION_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/dom_action.pb.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {
class ElementFinderResult;

// Action to get an element's status.
class GetElementStatusAction : public Action {
 public:
  explicit GetElementStatusAction(ActionDelegate* delegate,
                                  const ActionProto& proto);
  ~GetElementStatusAction() override;

  GetElementStatusAction(const GetElementStatusAction&) = delete;
  GetElementStatusAction& operator=(const GetElementStatusAction&) = delete;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void GetElementBySelector(const Selector& selector);
  void OnWaitForElement(const Selector& selector,
                        const ClientStatus& element_status);
  void GetElementByClientId(const ClientIdProto& client_id);
  void OnGetElement(const ClientStatus& status,
                    std::unique_ptr<ElementFinderResult> element);
  void OnGetStringAttribute(const ClientStatus& status,
                            const std::string& text);
  void OnResolveTextValue(const std::string& text,
                          const ClientStatus& status,
                          const std::string& value);
  void CompareResult(const std::string& text,
                     const std::string& value,
                     bool is_re2);

  void EndAction(const ClientStatus& status);

  std::unique_ptr<ElementFinderResult> element_;

  ProcessActionCallback callback_;
  base::WeakPtrFactory<GetElementStatusAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_GET_ELEMENT_STATUS_ACTION_H_
