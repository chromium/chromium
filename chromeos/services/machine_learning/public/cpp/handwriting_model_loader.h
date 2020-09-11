// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_HANDWRITING_MODEL_LOADER_H_
#define CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_HANDWRITING_MODEL_LOADER_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/services/machine_learning/public/mojom/handwriting_recognizer.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {
namespace machine_learning {

// Class that decides either to load handwriting model from rootfs or dlc.
// New Handwriting clients should call this helper instead of calling
// ServiceConnection::GetInstance()->LoadHandwritingModel.
// Three typical examples of the callstack are:
// Case 1: handwriting in enabled on rootfs.
//   client calls HandwritingModelLoader("en", receiver, callback).Load()
//   which calls LoadHandwritingModel -> handwriting model loaded from rootfs.
// Case 2: handwriting is enabled for dlc and dlc is already on the device.
//   client calls HandwritingModelLoader("en", receiver, callback).Load()
//   which calls -> GetExistingDlcs -> libhandwriting dlc already exists
//               -> InstallDlc -> LoadHandwritingModel
//   The correct handwriting model will be loaded and bond to the receiver.
// Case 3: handwriting is enabled for dlc and dlc is not on the device yet.
//   client calls HandwritingModelLoader("en", receiver, callback).Load()
//   which calls -> GetExistingDlcs -> NO libhandwriting dlc exists
//               -> Return error DLC_NOT_EXISTED.
//   Then it will be the client's duty to install the dlc and then calls
//   HandwritingModelLoader("en", receiver, callback).Load() again.
class HandwritingModelLoader {
 public:
  HandwritingModelLoader(
      mojom::HandwritingRecognizerSpecPtr spec,
      mojo::PendingReceiver<mojom::HandwritingRecognizer> receiver,
      mojom::MachineLearningService::LoadHandwritingModelCallback callback);

  ~HandwritingModelLoader();

  // Load handwriting model based on the comandline flag and language.
  void Load();

  static constexpr char kOndeviceHandwritingSwitch[] = "ondevice_handwriting";
  static constexpr char kLibHandwritingDlcId[] = "libhandwriting";

 private:
  friend class HandwritingModelLoaderTest;

  // Called when the existing-dlc-list is returned.
  // Returns an error if libhandwriting is not in the existing-dlc-list.
  // Calls InstallDlc otherwise.
  void OnGetExistingDlcsComplete(
      const std::string& err,
      const dlcservice::DlcsWithContent& dlcs_with_content);

  // Called when InstallDlc completes.
  // Returns an error if the `result.error` is not dlcservice::kErrorNone.
  // Calls mlservice to LoadHandwritingModel otherwise.
  void OnInstallDlcComplete(
      const chromeos::DlcserviceClient::InstallResult& result);

  DlcserviceClient* dlc_client_;
  mojom::HandwritingRecognizerSpecPtr spec_;
  mojo::PendingReceiver<mojom::HandwritingRecognizer> receiver_;
  mojom::MachineLearningService::LoadHandwritingModelCallback callback_;

  base::WeakPtrFactory<HandwritingModelLoader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HandwritingModelLoader);
};

}  // namespace machine_learning
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_HANDWRITING_MODEL_LOADER_H_
