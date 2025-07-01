// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_LOGGING_ML_LOG_ROUTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_LOGGING_ML_LOG_ROUTER_H_

#include "base/observer_list.h"
#include "base/scoped_observation_traits.h"
#include "components/autofill/core/browser/ml_model/logging/autofill_ml_internals.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

namespace autofill {

class MLLogReceiver : public base::CheckedObserver {
 public:
  virtual void ProcessLog(
      const autofill_ml_internals::mojom::MLPredictionLog& log) = 0;
};

class MLLogRouter : public KeyedService {
 public:
  MLLogRouter();
  MLLogRouter(const MLLogRouter&) = delete;
  MLLogRouter& operator=(const MLLogRouter&) = delete;
  ~MLLogRouter() override;

  void ProcessLog(autofill_ml_internals::mojom::MLPredictionLogPtr log);

  bool HasReceivers() const;

  void AddObserver(MLLogReceiver* receiver);
  void RemoveObserver(MLLogReceiver* receiver);

 private:
  base::ObserverList<MLLogReceiver, true> receivers_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_LOGGING_ML_LOG_ROUTER_H_
