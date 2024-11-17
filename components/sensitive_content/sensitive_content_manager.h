// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SENSITIVE_CONTENT_SENSITIVE_CONTENT_MANAGER_H_
#define COMPONENTS_SENSITIVE_CONTENT_SENSITIVE_CONTENT_MANAGER_H_

#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/autofill/content/browser/scoped_autofill_managers_observation.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/common/unique_ids.h"

namespace content {
class WebContents;
}  // namespace content

namespace sensitive_content {

class SensitiveContentClient;

// Contains platform-independent logic which tracks whether sensitive form
// fields are present or not. It is owned by the `SensitiveContentClient`.
class SensitiveContentManager final
    : public autofill::AutofillManager::Observer {
 public:
  SensitiveContentManager(content::WebContents* web_contents,
                          SensitiveContentClient* client);

  SensitiveContentManager(const SensitiveContentManager&) = delete;
  SensitiveContentManager& operator=(const SensitiveContentManager&) = delete;
  ~SensitiveContentManager() override;

 private:
  // Notifies the `SensitiveContentClient` if the content sensitivity has
  // changed (i.e. the current content sensitivity is different than
  // `last_content_was_sensitive_`). Returns true if the content sensitivity has
  // changed.
  bool UpdateContentSensitivity();

  // autofill::AutofillManager::Observer:

  // Adds the sensitive fields of `form_id` to `sensitive_fields_`. It is called
  // when fields receive predictions. It will be initially called with
  // heuristics predictions, and later called with server predictions.
  void OnFieldTypesDetermined(autofill::AutofillManager& manager,
                              autofill::FormGlobalId form_id,
                              FieldTypeSource) override;
  // Removes the fields of `removed_forms` from `sensitive_fields_`.
  // `removed_forms` are forms which have just been from the DOM.
  void OnBeforeFormsSeen(
      autofill::AutofillManager& manager,
      base::span<const autofill::FormGlobalId> updated_forms,
      base::span<const autofill::FormGlobalId> removed_forms) override;
  // Removes sensitive fields from `sensitive_fields_` when the manager becomes
  // not active (i.e. one of: inactive, pending reset or pending deletion). Adds
  // sensitive fields to `sensitive_fields_` when the manager becomes active
  // again.
  void OnAutofillManagerStateChanged(
      autofill::AutofillManager& manager,
      autofill::AutofillDriver::LifecycleState previous,
      autofill::AutofillDriver::LifecycleState current) override;

  // The last content sensitivity known by the `SensitiveContentClient` (by
  // default, initially, the content is considered not sensitive). Used to make
  // sure calls to the client are made only when the content sensitivity
  // changes, and not when it stays the same.
  bool last_content_was_sensitive_ = false;
  // Used to record the time elapsed between when the content became sensitive
  // and when the content became not sensitive again.
  std::optional<base::TimeTicks> content_became_sensitive_timestamp_;
  // Used to record the latency between when a form is first seen or last
  // modified, until the content is marked as sensitive.
  std::map<autofill::FormGlobalId, base::TimeTicks>
      latency_until_sensitive_timer_;

  const raw_ref<SensitiveContentClient> client_;
  std::set<autofill::FieldGlobalId> sensitive_fields_;

  autofill::ScopedAutofillManagersObservation autofill_managers_observation_{
      this};
};

}  // namespace sensitive_content

#endif  // COMPONENTS_SENSITIVE_CONTENT_SENSITIVE_CONTENT_MANAGER_H_
