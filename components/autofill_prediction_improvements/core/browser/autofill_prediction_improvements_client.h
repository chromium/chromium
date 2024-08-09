// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_CLIENT_H_
#define COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_CLIENT_H_

#include <string>

#include "base/functional/callback.h"

namespace autofill_prediction_improvements {

class AutofillPredictionImprovementsManager;

// An interface for embedder actions, e.g. Chrome on Desktop.
class AutofillPredictionImprovementsClient {
 public:
  // The callback to extract the page context.
  using PageContextCallback = base::OnceCallback<void(const std::string&)>;

  virtual ~AutofillPredictionImprovementsClient() = default;

  // Returns the page context. Which is a string summary of the page.
  virtual void GetPageContext(PageContextCallback callback) = 0;

  // Returns the `AutofillPredictionImprovements` associated with this client.
  virtual AutofillPredictionImprovementsManager& GetManager() = 0;
};

}  // namespace autofill_prediction_improvements

#endif  // COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_CLIENT_H_
