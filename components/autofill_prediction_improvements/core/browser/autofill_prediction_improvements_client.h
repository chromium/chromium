// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_CLIENT_H_
#define COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_CLIENT_H_

#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/autofill_prediction_improvements_delegate.h"

class GURL;

namespace optimization_guide::proto {
class AXTreeUpdate;
}

namespace user_annotations {
class UserAnnotationsService;
}

namespace autofill_prediction_improvements {

class AutofillPredictionImprovementsFillingEngine;
class AutofillPredictionImprovementsManager;

// An interface for embedder actions, e.g. Chrome on Desktop.
class AutofillPredictionImprovementsClient {
 public:
  // The callback to extract the accessibility tree snapshot.
  using AXTreeCallback =
      base::OnceCallback<void(optimization_guide::proto::AXTreeUpdate)>;

  virtual ~AutofillPredictionImprovementsClient() = default;

  // Calls `callback` with the accessibility tree snapshot.
  virtual void GetAXTree(AXTreeCallback callback) = 0;

  // Returns the `AutofillPredictionImprovementsManager` associated with this
  // client.
  virtual AutofillPredictionImprovementsManager& GetManager() = 0;

  // Returns the filling engine associated with the client's web contents.
  // TODO(crbug.com/372432481): Make this return a reference.
  virtual AutofillPredictionImprovementsFillingEngine* GetFillingEngine() = 0;

  // Returns the last committed URL of the primary main frame.
  virtual const GURL& GetLastCommittedURL() = 0;

  // Returns the title of the web contents.
  virtual std::string GetTitle() = 0;

  // Returns a pointer to the current profile's `UserAnnotationsService`. Can be
  // `nullptr`.
  virtual user_annotations::UserAnnotationsService*
  GetUserAnnotationsService() = 0;

  // Returns whether the feature is enabled in the prefs
  // (`autofill::prefs::kAutofillPredictionImprovementsEnabled`).
  //
  // This is different from `IsAutofillPredictionImprovementsSupported()`, which
  // checks if the user could enable the feature in the first case (if not, the
  // client is not instantiated in the first place).
  virtual bool IsAutofillPredictionImprovementsEnabledPref() const = 0;

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

  // Returns the Autofill filling value for `field` for the Autofill profile
  // identified by `autofill_profile_guid`, if any.
  virtual std::u16string GetAutofillFillingValue(
      const std::string& autofill_profile_guid,
      autofill::FieldType field_type,
      const autofill::FormFieldData& field) = 0;
};

}  // namespace autofill_prediction_improvements

#endif  // COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_CLIENT_H_
