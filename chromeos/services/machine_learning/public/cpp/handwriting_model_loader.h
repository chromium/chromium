// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_HANDWRITING_MODEL_LOADER_H_
#define CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_HANDWRITING_MODEL_LOADER_H_

#include "chromeos/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/services/machine_learning/public/mojom/handwriting_recognizer.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {
namespace machine_learning {

// Helper function decides either to load handwriting model from rootfs or dlc.
// New Handwriting clients should call this helper instead of calling
// ServiceConnection::GetInstance()->LoadHandwritingModel.
// Three typical examples of the callstack are:
// Case 1: handwriting in enabled on rootfs.
//   client calls LoadHandwritingModelFromRootfsOrDlc("en", receiver, callback)
//   which calls LoadHandwritingModel -> handwriting model loaded from rootfs.
// Case 2: handwriting is enabled for dlc and dlc is already on the device.
//   client calls LoadHandwritingModelFromRootfsOrDlc("en", receiver, callback)
//   which calls -> GetExistingDlcs -> libhandwriting dlc already exists
//               -> InstallDlc -> LoadHandwritingModel
//   The correct handwriting model will be loaded and bond to the receiver.
// Case 3: handwriting is enabled for dlc and dlc is not on the device yet.
//   client calls LoadHandwritingModelFromRootfsOrDlc("en", receiver, callback)
//   which calls -> GetExistingDlcs -> NO libhandwriting dlc exists
//               -> Return error DLC_NOT_EXISTED.
//   Then it will be the client's duty to install the dlc and then calls
//   LoadHandwritingModelFromRootfsOrDlc("en", receiver, callback) again.
//
//  `dlc_client` should only be replaced with non-default value in unit tests.
void LoadHandwritingModelFromRootfsOrDlc(
    mojom::HandwritingRecognizerSpecPtr spec,
    mojo::PendingReceiver<mojom::HandwritingRecognizer> receiver,
    mojom::MachineLearningService::LoadHandwritingModelCallback callback,
    DlcserviceClient* dlc_client = chromeos::DlcserviceClient::Get());

}  // namespace machine_learning
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_HANDWRITING_MODEL_LOADER_H_
