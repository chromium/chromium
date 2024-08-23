// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SENSITIVE_CONTENT_SENSITIVE_CONTENT_MANAGER_H_
#define COMPONENTS_SENSITIVE_CONTENT_SENSITIVE_CONTENT_MANAGER_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/content/browser/scoped_autofill_managers_observation.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/common/unique_ids.h"

namespace content {
class WebContents;
}  // namespace content

namespace sensitive_content {

class SensitiveContentClient;

// Contains platform-independent logic which tracks whether sensitive form
// fields are present or not. It is owned by the embedder-specific
// implementation of `SensitiveContentClient`.
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
  // `last_content_was_sensitive_`).
  void UpdateContentSensitivity();

  // autofill::AutofillManager::Observer:

  // Notified when fields receive predictions. It will be initially called with
  // heuristics predictions, and later called with server predictions.
  void OnFieldTypesDetermined(autofill::AutofillManager& manager,
                              autofill::FormGlobalId form,
                              FieldTypeSource) override;
  // Notified about the forms removed from the DOM.
  void OnBeforeFormsSeen(
      autofill::AutofillManager& manager,
      base::span<const autofill::FormGlobalId> updated_forms,
      base::span<const autofill::FormGlobalId> removed_forms) override;

  // The last content sensitivity known by the `SensitiveContentClient` (by
  // default, initially, the content is considered not sensitive). Used to make
  // sure calls to the client are made only when the content sensitivity
  // changes, and not when it stays the same.
  bool last_content_was_sensitive_ = false;
  const raw_ref<SensitiveContentClient> client_;
  std::set<autofill::FieldGlobalId> sensitive_fields_;
  autofill::ScopedAutofillManagersObservation autofill_managers_observation_{
      this};
};

}  // namespace sensitive_content

#endif  // COMPONENTS_SENSITIVE_CONTENT_SENSITIVE_CONTENT_MANAGER_H_
