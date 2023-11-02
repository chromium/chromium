// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_PERFORM_ON_SINGLE_ELEMENT_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_PERFORM_ON_SINGLE_ELEMENT_ACTION_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/dom_action.pb.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"

namespace autofill_assistant {

// An action that runs the registered execution callback on a single element
// previously stored. No additional parameters are provided when running the
// callback.
class PerformOnSingleElementAction : public Action {
 public:
  using PerformAction =
      base::OnceCallback<void(const ElementFinderResult&,
                              base::OnceCallback<void(const ClientStatus&)>)>;
  using PerformTimedAction = base::OnceCallback<void(
      const ElementFinderResult&,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>)>;

  ~PerformOnSingleElementAction() override;

  PerformOnSingleElementAction(const PerformOnSingleElementAction&) = delete;
  PerformOnSingleElementAction& operator=(const PerformOnSingleElementAction&) =
      delete;

  static std::unique_ptr<PerformOnSingleElementAction> WithClientId(
      ActionDelegate* delegate,
      const ActionProto& proto,
      const ClientIdProto& client_id,
      PerformAction perform);
  static std::unique_ptr<PerformOnSingleElementAction> WithOptionalClientId(
      ActionDelegate* delegate,
      const ActionProto& proto,
      const ClientIdProto& client_id,
      PerformAction perform);
  static std::unique_ptr<PerformOnSingleElementAction> WithClientIdTimed(
      ActionDelegate* delegate,
      const ActionProto& proto,
      const ClientIdProto& client_id,
      PerformTimedAction perform);
  static std::unique_ptr<PerformOnSingleElementAction>
  WithOptionalClientIdTimed(ActionDelegate* delegate,
                            const ActionProto& proto,
                            const ClientIdProto& client_id,
                            PerformTimedAction perform);

 private:
  PerformOnSingleElementAction(ActionDelegate* delegate,
                               const ActionProto& proto,
                               const ClientIdProto& client_id,
                               bool element_is_optional,
                               PerformAction perform,
                               PerformTimedAction perform_timed);

  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void EndAction(const ClientStatus& status);

  ElementFinderResult element_;
  ProcessActionCallback callback_;

  std::string client_id_;
  bool element_is_optional_ = false;
  PerformAction perform_;
  PerformTimedAction perform_timed_;

  base::WeakPtrFactory<PerformOnSingleElementAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_PERFORM_ON_SINGLE_ELEMENT_ACTION_H_
