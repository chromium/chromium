// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_CHECK_ELEMENT_TAG_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_CHECK_ELEMENT_TAG_ACTION_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/dom_action.pb.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"

namespace autofill_assistant {

// Action to check whether or not an element matches the expected set of tags.
class CheckElementTagAction : public Action {
 public:
  explicit CheckElementTagAction(ActionDelegate* delegate,
                                 const ActionProto& proto);
  ~CheckElementTagAction() override;

  CheckElementTagAction(const CheckElementTagAction&) = delete;
  CheckElementTagAction& operator=(const CheckElementTagAction&) = delete;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void OnGetElementTag(const ClientStatus& status, const std::string& tag);

  void EndAction(const ClientStatus& status);

  ElementFinderResult element_;
  ProcessActionCallback callback_;

  base::WeakPtrFactory<CheckElementTagAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_CHECK_ELEMENT_TAG_ACTION_H_
