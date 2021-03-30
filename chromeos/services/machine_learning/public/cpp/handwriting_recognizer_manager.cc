// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/machine_learning/public/cpp/handwriting_recognizer_manager.h"

#include "base/no_destructor.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/handwriting_recognizer.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/handwriting_recognizer_requestor.mojom.h"

namespace chromeos {
namespace machine_learning {

HandwritingRecognizerManager::HandwritingRecognizerManager() = default;

HandwritingRecognizerManager::~HandwritingRecognizerManager() = default;

HandwritingRecognizerManager* HandwritingRecognizerManager::GetInstance() {
  static base::NoDestructor<HandwritingRecognizerManager> manager;
  return manager.get();
}

void HandwritingRecognizerManager::AddReceiver(
    mojo::PendingReceiver<mojom::HandwritingRecognizerRequestor> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void HandwritingRecognizerManager::LoadHandwritingModel(
    mojo::PendingReceiver<mojom::HandwritingRecognizer> receiver,
    LoadHandwritingModelCallback callback) {
  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadHandwritingModelWithSpec(mojom::HandwritingRecognizerSpec::New("en"),
                                    std::move(receiver), std::move(callback));
}

void HandwritingRecognizerManager::LoadGestureModel(
    mojo::PendingReceiver<mojom::HandwritingRecognizer> receiver,
    LoadHandwritingModelCallback callback) {
  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadHandwritingModelWithSpec(
          mojom::HandwritingRecognizerSpec::New("gesture_in_context"),
          std::move(receiver), std::move(callback));
}

}  // namespace machine_learning
}  // namespace chromeos
