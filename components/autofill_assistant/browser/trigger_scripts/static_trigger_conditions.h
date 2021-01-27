// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_STATIC_TRIGGER_CONDITIONS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_STATIC_TRIGGER_CONDITIONS_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/website_login_manager.h"
#include "url/gurl.h"

namespace autofill_assistant {

// Provides easy access to the values of static trigger conditions. Static
// trigger conditions do not depend on the current state of the DOM, as opposed
// to dynamic element conditions.
class StaticTriggerConditions {
 public:
  StaticTriggerConditions();
  virtual ~StaticTriggerConditions();

  // Initializes the field values using |website_login_manager| and
  // |is_first_time_user_callback|. Invokes |callback| when done. All parameters
  // must outlive this call.
  virtual void Init(
      WebsiteLoginManager* website_login_manager,
      base::RepeatingCallback<bool(void)> is_first_time_user_callback,
      const GURL& url,
      TriggerContext* trigger_context,
      base::OnceCallback<void(void)> callback);
  virtual void set_is_first_time_user(bool first_time_user);
  virtual bool is_first_time_user() const;
  virtual bool has_stored_login_credentials() const;
  virtual bool is_in_experiment(int experiment_id) const;
  virtual bool script_parameter_matches(
      const ScriptParameterMatchProto& param) const;

  // If true, all values have been evaluated. They may be out-of-date by one
  // cycle in case an update is currently scheduled.
  virtual bool has_results() const;

 private:
  void OnGetLogins(std::vector<WebsiteLoginManager::Login> logins);

  // The callback to invoke once all information requested in |Init| has been
  // collected. Only valid during calls of |Init|.
  base::OnceCallback<void(void)> callback_;
  bool has_results_ = false;
  bool is_first_time_user_ = true;
  bool has_stored_login_credentials_ = false;
  TriggerContext* trigger_context_;
  base::WeakPtrFactory<StaticTriggerConditions> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_STATIC_TRIGGER_CONDITIONS_H_
