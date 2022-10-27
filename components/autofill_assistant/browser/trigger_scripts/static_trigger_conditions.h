// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_STATIC_TRIGGER_CONDITIONS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_STATIC_TRIGGER_CONDITIONS_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/public/password_change/website_login_manager.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "url/gurl.h"

namespace autofill_assistant {
class StarterPlatformDelegate;

// Provides easy access to the values of static trigger conditions. Static
// trigger conditions do not depend on the current state of the DOM, as opposed
// to dynamic element conditions.
class StaticTriggerConditions {
 public:
  // |delegate| and |trigger_context| must outlive this instance.
  StaticTriggerConditions(base::WeakPtr<StarterPlatformDelegate> delegate,
                          TriggerContext* trigger_context,
                          const GURL& deeplink_url);
  virtual ~StaticTriggerConditions();

  // Updates/initializes the static trigger conditions. Invokes |callback| when
  // done.
  virtual void Update(base::OnceCallback<void(void)> callback);
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
  bool has_stored_login_credentials_ = false;
  // Note: this is cached to ensure that the flag value is consistent until the
  // next call to |Update|. See b/192220992.
  bool is_first_time_user_ = false;
  base::WeakPtr<StarterPlatformDelegate> delegate_;
  raw_ptr<TriggerContext, DanglingUntriaged> trigger_context_ = nullptr;
  GURL deeplink_url_;
  base::WeakPtrFactory<StaticTriggerConditions> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_STATIC_TRIGGER_CONDITIONS_H_
