// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/machine_learning/cpp/ash/handwriting_model_loader.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/services/machine_learning/public/cpp/ml_switches.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {
namespace machine_learning {
namespace {

using ::chromeos::machine_learning::mojom::HandwritingRecognizerSpecPtr;
using ::chromeos::machine_learning::mojom::LoadHandwritingModelResult;
using HandwritingRecognizer = mojo::PendingReceiver<
    ::chromeos::machine_learning::mojom::HandwritingRecognizer>;
using LoadHandwritingModelCallback = ::chromeos::machine_learning::mojom::
    MachineLearningService::LoadHandwritingModelCallback;

// Records CrOSActionRecorder event.
void RecordLoadHandwritingModelResult(const LoadHandwritingModelResult val) {
  UMA_HISTOGRAM_ENUMERATION(
      "MachineLearningService.HandwritingModel.LoadModelResult.Event", val,
      LoadHandwritingModelResult::LOAD_MODEL_FILES_ERROR);
}

constexpr char kLibHandwritingDlcId[] = "libhandwriting";
// A list of supported language code.
constexpr char kLanguageCodeEn[] = "en";
constexpr char kLanguageCodeGesture[] = "gesture_in_context";

// Returns whether the `value` is set for command line switch
// kOndeviceHandwritingSwitch.
bool HandwritingSwitchHasValue(const std::string& value) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(::switches::kOndeviceHandwritingSwitch) &&
         command_line->GetSwitchValueASCII(
             switches::kOndeviceHandwritingSwitch) == value;
}

// Returns true if switch kOndeviceHandwritingSwitch is set to use_rootfs.
bool IsLibHandwritingRootfsEnabled() {
  return HandwritingSwitchHasValue("use_rootfs");
}

// Returns true if switch kOndeviceHandwritingSwitch is set to use_dlc.
bool IsLibHandwritingDlcEnabled() {
  return HandwritingSwitchHasValue("use_dlc");
}

// Called when InstallDlc completes.
// Returns an error if the `result.error` is not dlcservice::kErrorNone.
// Calls mlservice to LoadHandwritingModel otherwise.
void OnInstallDlcComplete(HandwritingRecognizerSpecPtr spec,
                          HandwritingRecognizer receiver,
                          LoadHandwritingModelCallback callback,
                          const DlcserviceClient::InstallResult& result) {
  // Call LoadHandwritingModelWithSpec if no error was found.
  if (result.error == dlcservice::kErrorNone) {
    chromeos::machine_learning::ServiceConnection::GetInstance()
        ->GetMachineLearningService()
        .LoadHandwritingModel(std::move(spec), std::move(receiver),
                              std::move(callback));
    return;
  }

  RecordLoadHandwritingModelResult(
      LoadHandwritingModelResult::DLC_INSTALL_ERROR);
  std::move(callback).Run(LoadHandwritingModelResult::DLC_INSTALL_ERROR);
}

// Called when the existing-dlc-list is returned.
// Returns an error if libhandwriting is not in the existing-dlc-list.
// Calls InstallDlc otherwise.
void OnGetExistingDlcsComplete(
    HandwritingRecognizerSpecPtr spec,
    HandwritingRecognizer receiver,
    LoadHandwritingModelCallback callback,
    DlcserviceClient* const dlc_client,
    std::string_view err,
    const dlcservice::DlcsWithContent& dlcs_with_content) {
  // Loop over dlcs_with_content, and installs libhandwriting if already exists.
  // Since we don't want to trigger downloading here, we only install(mount)
  // the handwriting dlc if it is already on device.
  for (const auto& dlc_info : dlcs_with_content.dlc_infos()) {
    if (dlc_info.id() == kLibHandwritingDlcId) {
      dlcservice::InstallRequest install_request;
      install_request.set_id(kLibHandwritingDlcId);
      dlc_client->Install(
          install_request,
          base::BindOnce(&OnInstallDlcComplete, std::move(spec),
                         std::move(receiver), std::move(callback)),
          base::DoNothing());
      return;
    }
  }

  // Returns error if the handwriting dlc is not on the device.
  RecordLoadHandwritingModelResult(
      LoadHandwritingModelResult::DLC_DOES_NOT_EXIST);
  std::move(callback).Run(LoadHandwritingModelResult::DLC_DOES_NOT_EXIST);
}

}  // namespace

void LoadHandwritingModelFromRootfsOrDlc(HandwritingRecognizerSpecPtr spec,
                                         HandwritingRecognizer receiver,
                                         LoadHandwritingModelCallback callback,
                                         DlcserviceClient* const dlc_client) {
  // Returns FEATURE_NOT_SUPPORTED_ERROR if both rootfs and dlc are not enabled.
  if (!IsLibHandwritingRootfsEnabled() && !IsLibHandwritingDlcEnabled()) {
    RecordLoadHandwritingModelResult(
        LoadHandwritingModelResult::FEATURE_NOT_SUPPORTED_ERROR);
    std::move(callback).Run(
        LoadHandwritingModelResult::FEATURE_NOT_SUPPORTED_ERROR);
    return;
  }

  // Returns LANGUAGE_NOT_SUPPORTED_ERROR if the language is not supported yet.
  if (spec->language != kLanguageCodeEn &&
      spec->language != kLanguageCodeGesture) {
    RecordLoadHandwritingModelResult(
        LoadHandwritingModelResult::LANGUAGE_NOT_SUPPORTED_ERROR);
    std::move(callback).Run(
        LoadHandwritingModelResult::LANGUAGE_NOT_SUPPORTED_ERROR);
    return;
  }

  // Load from rootfs if enabled.
  if (IsLibHandwritingRootfsEnabled()) {
    chromeos::machine_learning::ServiceConnection::GetInstance()
        ->GetMachineLearningService()
        .LoadHandwritingModel(std::move(spec), std::move(receiver),
                              std::move(callback));
    return;
  }

  // Gets existing dlc list and based on the presence of libhandwriting
  // either returns an error or installs the libhandwriting dlc.
  dlc_client->GetExistingDlcs(
      base::BindOnce(&OnGetExistingDlcsComplete, std::move(spec),
                     std::move(receiver), std::move(callback), dlc_client));
}

}  // namespace machine_learning
}  // namespace ash
