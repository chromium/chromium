// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/bluetooth/fast_pair_software_scanning_handler.h"

#include "base/functional/bind.h"

namespace {

const char kReceiveBatterySaverStatusMessage[] = "requestBatterySaverStatus";
const char kReceiveHardwareOffloadingStatusMessage[] =
    "requestHardwareOffloadingSupportStatus";
const char kSendBatterySaverStatusMessage[] =
    "fast-pair-software-scanning-battery-saver-status";
const char kSendHardwareOffloadingStatusMessage[] =
    "fast-pair-software-scanning-hardware-offloading-status";

}  // namespace

namespace ash::settings {

FastPairSoftwareScanningHandler::FastPairSoftwareScanningHandler(
    std::unique_ptr<ash::quick_pair::BatterySaverActiveProvider>
        battery_saver_active_provider,
    std::unique_ptr<ash::quick_pair::HardwareOffloadingSupportedProvider>
        hardware_offloading_supported_provider)
    : battery_saver_active_provider_(std::move(battery_saver_active_provider)),
      hardware_offloading_supported_provider_(
          std::move(hardware_offloading_supported_provider)) {
  if (battery_saver_active_provider_) {
    battery_saver_active_provider_->SetCallback(base::BindRepeating(
        &FastPairSoftwareScanningHandler::OnBatterySaverActiveStatusChange,
        weak_factory_.GetWeakPtr()));
  }

  if (hardware_offloading_supported_provider_) {
    hardware_offloading_supported_provider_->SetCallback(
        base::BindRepeating(&FastPairSoftwareScanningHandler::
                                OnHardwareOffloadingSupportedStatusChange,
                            weak_factory_.GetWeakPtr()));
  }
}

FastPairSoftwareScanningHandler::~FastPairSoftwareScanningHandler() = default;

void FastPairSoftwareScanningHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kReceiveBatterySaverStatusMessage,
      base::BindRepeating(&FastPairSoftwareScanningHandler::
                              HandleBatterySaverActiveStatusRequest,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      kReceiveHardwareOffloadingStatusMessage,
      base::BindRepeating(&FastPairSoftwareScanningHandler::
                              HandleHardwareOffloadingSupportStatusRequest,
                          base::Unretained(this)));
}

void FastPairSoftwareScanningHandler::OnJavascriptAllowed() {}

void FastPairSoftwareScanningHandler::OnJavascriptDisallowed() {}

void FastPairSoftwareScanningHandler::HandleBatterySaverActiveStatusRequest(
    const base::Value::List& args) {
  AllowJavascript();
  if (battery_saver_active_provider_) {
    FireWebUIListener(
        kSendBatterySaverStatusMessage,
        base::Value(battery_saver_active_provider_->is_enabled()));
  }
}

void FastPairSoftwareScanningHandler::
    HandleHardwareOffloadingSupportStatusRequest(
        const base::Value::List& args) {
  AllowJavascript();
  if (hardware_offloading_supported_provider_) {
    FireWebUIListener(
        kSendHardwareOffloadingStatusMessage,
        base::Value(hardware_offloading_supported_provider_->is_enabled()));
  }
}

void FastPairSoftwareScanningHandler::OnBatterySaverActiveStatusChange(
    bool is_enabled) {
  AllowJavascript();
  FireWebUIListener(kSendBatterySaverStatusMessage, base::Value(is_enabled));
}

void FastPairSoftwareScanningHandler::OnHardwareOffloadingSupportedStatusChange(
    bool is_enabled) {
  AllowJavascript();
  FireWebUIListener(kSendHardwareOffloadingStatusMessage,
                    base::Value(is_enabled));
}

}  // namespace ash::settings
