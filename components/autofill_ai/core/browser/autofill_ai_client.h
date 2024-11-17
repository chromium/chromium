// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_CLIENT_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_CLIENT_H_

#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/autofill_ai_delegate.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/user_annotations/user_annotations_types.h"

class GURL;

namespace optimization_guide::proto {
class AXTreeUpdate;
}

namespace url {
class Origin;
}  // namespace url

namespace user_annotations {
class UserAnnotationsService;
}

namespace autofill_ai {

class AutofillAiModelExecutor;
class AutofillAiManager;

// An interface for embedder actions, e.g. Chrome on Desktop.
//
// A client should be created only if
// `IsAutofillAiSupported()`. However,
// `IsAutofillAiSupported()` is not necessarily a constant
// over the lifetime of the client. For example, the user may disable Autofill
// in the settings while the client is alive.
class AutofillAiClient {
 public:
  // The callback to extract the accessibility tree snapshot.
  using AXTreeCallback =
      base::OnceCallback<void(optimization_guide::proto::AXTreeUpdate)>;

  virtual ~AutofillAiClient() = default;

  // Returns the AutofillClient that is scoped to the same object (e.g., tab) as
  // this AutofillAiClient.
  virtual autofill::AutofillClient& GetAutofillClient() = 0;

  // Calls `callback` with the accessibility tree snapshot.
  virtual void GetAXTree(AXTreeCallback callback) = 0;

  // Returns the `AutofillAiManager` associated with this
  // client.
  virtual AutofillAiManager& GetManager() = 0;

  // Returns the Autofill AI model executor associated with the client's web
  // contents.
  // TODO(crbug.com/372432481): Make this return a reference.
  virtual AutofillAiModelExecutor* GetModelExecutor() = 0;

  // Returns the last committed URL of the primary main frame.
  virtual const GURL& GetLastCommittedURL() = 0;

  // Returns the last committed origin of the primary main frame.
  virtual const url::Origin& GetLastCommittedOrigin() = 0;

  // Returns the title of the web contents.
  virtual std::string GetTitle() = 0;

  // Returns a pointer to the current profile's `UserAnnotationsService`. Can be
  // `nullptr`.
  virtual user_annotations::UserAnnotationsService*
  GetUserAnnotationsService() = 0;

  // Returns whether the feature is enabled in the prefs
  // (`autofill::prefs::kAutofillAiEnabled`).
  //
  // This is different from `IsAutofillAiSupported()`, which
  // checks if the user could enable the feature in the first case (if not, the
  // client is not instantiated in the first place).
  virtual bool IsAutofillAiEnabledPref() const = 0;

  // Opens the feedback page if the feature is allowed for feedback.
  virtual void TryToOpenFeedbackPage(const std::string& feedback_id) = 0;

  // Opens the settings page for prediction improvements.
  virtual void OpenPredictionImprovementsSettings() = 0;

  // Returns whether the current user is eligible for the improved prediction
  // experience.
  virtual bool IsUserEligible() = 0;

  // Returns a pointer to a FormStructure for the corresponding `form_data`
  // from the Autofill cache. Can be a `nullptr` when the structure was not
  // found or if the driver is not available.
  virtual autofill::FormStructure* GetCachedFormStructure(
      const autofill::FormData& form_data) = 0;

  // Returns the Autofill filling value for `field` of type `field_type` for the
  // Autofill profile identified by `autofill_profile_guid`, if any. Only
  // supports name types, and returns an empty string for all other types.
  virtual std::u16string GetAutofillNameFillingValue(
      const std::string& autofill_profile_guid,
      autofill::FieldType field_type,
      const autofill::FormFieldData& field) = 0;

  // Shows a bubble asking whether the user wants to save prediction
  // improvements data.
  virtual void ShowSaveAutofillAiBubble(
      std::unique_ptr<user_annotations::FormAnnotationResponse>
          form_annotation_response,
      user_annotations::PromptAcceptanceCallback
          prompt_acceptance_callback) = 0;
};

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_CLIENT_H_
