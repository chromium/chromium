// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/machine_learning/public/cpp/handwriting_model_loader.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace machine_learning {
namespace {

using chromeos::machine_learning::mojom::LoadHandwritingModelResult;

// Records CrOSActionRecorder event.
void RecordLoadHandwritingModelResult(const LoadHandwritingModelResult val) {
  UMA_HISTOGRAM_ENUMERATION(
      "MachineLearningService.HandwritingModel.LoadModelResult.Event", val,
      LoadHandwritingModelResult::LOAD_MODEL_FILES_ERROR);
}

// A list of supported language code.
constexpr char kLanguageCodeEn[] = "en";
constexpr char kLanguageCodeGesture[] = "gesture_in_context";

// Returns whether the `value` is set for command line switch
// kOndeviceHandwritingSwitch.
bool HandwritingSwitchHasValue(const std::string& value) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(
             HandwritingModelLoader::kOndeviceHandwritingSwitch) &&
         command_line->GetSwitchValueASCII(
             HandwritingModelLoader::kOndeviceHandwritingSwitch) == value;
}

// Returns true if switch kOndeviceHandwritingSwitch is set to use_rootfs.
bool IsLibHandwritingRootfsEnabled() {
  return HandwritingSwitchHasValue("use_rootfs");
}

// Returns true if switch kOndeviceHandwritingSwitch is set to use_dlc.
bool IsLibHandwritingDlcEnabled() {
  return HandwritingSwitchHasValue("use_dlc");
}

}  // namespace

constexpr char HandwritingModelLoader::kOndeviceHandwritingSwitch[];
constexpr char HandwritingModelLoader::kLibHandwritingDlcId[];

HandwritingModelLoader::HandwritingModelLoader(
    mojom::HandwritingRecognizerSpecPtr spec,
    mojo::PendingReceiver<mojom::HandwritingRecognizer> receiver,
    mojom::MachineLearningService::LoadHandwritingModelCallback callback)
    : dlc_client_(chromeos::DlcserviceClient::Get()),
      spec_(std::move(spec)),
      receiver_(std::move(receiver)),
      callback_(std::move(callback)),
      weak_ptr_factory_(this) {}

HandwritingModelLoader::~HandwritingModelLoader() = default;

void HandwritingModelLoader::Load() {
  // Returns FEATURE_NOT_SUPPORTED_ERROR if both rootfs and dlc are not enabled.
  if (!IsLibHandwritingRootfsEnabled() && !IsLibHandwritingDlcEnabled()) {
    RecordLoadHandwritingModelResult(
        LoadHandwritingModelResult::FEATURE_NOT_SUPPORTED_ERROR);
    std::move(callback_).Run(
        LoadHandwritingModelResult::FEATURE_NOT_SUPPORTED_ERROR);
    return;
  }

  // Returns LANGUAGE_NOT_SUPPORTED_ERROR if the language is not supported yet.
  if (spec_->language != kLanguageCodeEn &&
      spec_->language != kLanguageCodeGesture) {
    RecordLoadHandwritingModelResult(
        LoadHandwritingModelResult::LANGUAGE_NOT_SUPPORTED_ERROR);
    std::move(callback_).Run(
        LoadHandwritingModelResult::LANGUAGE_NOT_SUPPORTED_ERROR);
    return;
  }

  // Load from rootfs if enabled.
  if (IsLibHandwritingRootfsEnabled()) {
    ServiceConnection::GetInstance()->LoadHandwritingModel(
        std::move(spec_), std::move(receiver_), std::move(callback_));
    return;
  }

  // Gets existing dlc list and based on the presence of libhandwriting
  // either returns an error or installs the libhandwriting dlc.
  dlc_client_->GetExistingDlcs(
      base::BindOnce(&HandwritingModelLoader::OnGetExistingDlcsComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HandwritingModelLoader::OnGetExistingDlcsComplete(
    const std::string& err,
    const dlcservice::DlcsWithContent& dlcs_with_content) {
  // Loop over dlcs_with_content, and installs libhandwriting if already exists.
  // Since we don't want to trigger downloading here, we only install(mount)
  // the handwriting dlc if it is already on device.
  for (const auto& dlc_info : dlcs_with_content.dlc_infos()) {
    if (dlc_info.id() == HandwritingModelLoader::kLibHandwritingDlcId) {
      dlc_client_->Install(
          kLibHandwritingDlcId,
          base::BindOnce(&HandwritingModelLoader::OnInstallDlcComplete,
                         weak_ptr_factory_.GetWeakPtr()),
          chromeos::DlcserviceClient::IgnoreProgress);
      return;
    }
  }

  // Returns error if the handwriting dlc is not on the device.
  RecordLoadHandwritingModelResult(
      LoadHandwritingModelResult::DLC_DOES_NOT_EXIST);
  std::move(callback_).Run(LoadHandwritingModelResult::DLC_DOES_NOT_EXIST);
}

void HandwritingModelLoader::OnInstallDlcComplete(
    const chromeos::DlcserviceClient::InstallResult& result) {
  // Call LoadHandwritingModelWithSpec if no error was found.
  if (result.error == dlcservice::kErrorNone) {
    ServiceConnection::GetInstance()->LoadHandwritingModel(
        std::move(spec_), std::move(receiver_), std::move(callback_));
    return;
  }

  RecordLoadHandwritingModelResult(
      LoadHandwritingModelResult::DLC_INSTALL_ERROR);
  std::move(callback_).Run(LoadHandwritingModelResult::DLC_INSTALL_ERROR);
}

}  // namespace machine_learning
}  // namespace chromeos
