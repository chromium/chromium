// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_CLIENT_H_
#define COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_CLIENT_H_

#include "base/functional/callback_forward.h"

namespace optimization_guide::proto {
class AXTreeUpdate;
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
  virtual AutofillPredictionImprovementsFillingEngine* GetFillingEngine() = 0;
};

}  // namespace autofill_prediction_improvements

#endif  // COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_CLIENT_H_
