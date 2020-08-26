// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MACHINE_LEARNING_MACHINE_LEARNING_SERVICE_H_
#define CHROME_SERVICES_MACHINE_LEARNING_MACHINE_LEARNING_SERVICE_H_

#include "chrome/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace machine_learning {

// mojom::MachineLearningService implementation.
class MachineLearningService : public mojom::MachineLearningService {
 public:
  // Creates an instance which binds to |receiver|. |receiver| will be consumed
  // and must be valid.
  explicit MachineLearningService(
      mojo::PendingReceiver<mojom::MachineLearningService> receiver);
  ~MachineLearningService() override;

  MachineLearningService(const MachineLearningService&) = delete;
  MachineLearningService& operator=(const MachineLearningService&) = delete;

 private:
  // |mojom::MachineLearningService::LoadDecisionTree| override.
  void LoadDecisionTree(
      mojom::DecisionTreeModelSpecPtr spec,
      mojo::PendingReceiver<mojom::DecisionTreePredictor> receiver,
      LoadDecisionTreeCallback callback) override;

  // Service-side Mojo receiver endpoint.
  mojo::Receiver<mojom::MachineLearningService> receiver_;
};

}  // namespace machine_learning

#endif  // CHROME_SERVICES_MACHINE_LEARNING_MACHINE_LEARNING_SERVICE_H_
