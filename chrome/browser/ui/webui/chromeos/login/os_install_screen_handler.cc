// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/os_install_screen_handler.h"

#include <string>

#include "base/notreached.h"
#include "chrome/browser/ash/login/screens/os_install_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/js_calls_container.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {
constexpr const char kConfirmStep[] = "confirm";
constexpr const char kInProgressStep[] = "in-progress";
constexpr const char kFailedStep[] = "failed";
constexpr const char kNoDestinationDeviceFoundStep[] =
    "no-destination-device-found";
constexpr const char kSuccessStep[] = "success";
}  // namespace

// static
constexpr StaticOobeScreenId OsInstallScreenView::kScreenId;

OsInstallScreenHandler::OsInstallScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.OsInstallScreen.userActed");
}

OsInstallScreenHandler::~OsInstallScreenHandler() {
  OsInstallClient::Get()->RemoveObserver(this);
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void OsInstallScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("osInstallDialogIntroTitle", IDS_OS_INSTALL_SCREEN_INTRO_TITLE);
  builder->Add("osInstallDialogIntroSubtitle",
               IDS_OS_INSTALL_SCREEN_INTRO_SUBTITLE);
  builder->Add("osInstallDialogIntroBody", IDS_OS_INSTALL_SCREEN_INTRO_BODY);
  builder->Add("osInstallDialogIntroNextButton",
               IDS_OS_INSTALL_SCREEN_INTRO_NEXT_BUTTON);

  builder->Add("osInstallDialogConfirmTitle",
               IDS_OS_INSTALL_SCREEN_CONFIRM_TITLE);
  builder->Add("osInstallDialogConfirmBody",
               IDS_OS_INSTALL_SCREEN_CONFIRM_BODY);
  builder->Add("osInstallDialogConfirmNextButton",
               IDS_OS_INSTALL_SCREEN_CONFIRM_NEXT_BUTTON);

  builder->Add("osInstallDialogInProgressTitle",
               IDS_OS_INSTALL_SCREEN_IN_PROGRESS_TITLE);
  builder->Add("osInstallDialogInProgressSubtitle",
               IDS_OS_INSTALL_SCREEN_IN_PROGRESS_SUBTITLE);

  builder->Add("osInstallDialogErrorTitle", IDS_OS_INSTALL_SCREEN_ERROR_TITLE);
  builder->Add("osInstallDialogErrorFailedSubtitle",
               IDS_OS_INSTALL_SCREEN_ERROR_FAILED_SUBTITLE);
  builder->Add("osInstallDialogErrorNoDestSubtitle",
               IDS_OS_INSTALL_SCREEN_ERROR_NO_DEST_SUBTITLE);
  builder->Add("osInstallDialogErrorNoDestContent",
               IDS_OS_INSTALL_SCREEN_ERROR_NO_DEST_CONTENT);
  builder->Add("osInstallDialogErrorServiceLogsLink",
               IDS_OS_INSTALL_SCREEN_ERROR_SERVICE_LOGS_LINK_TEXT);
  builder->Add("osInstallDialogServiceLogsTitle",
               IDS_OS_INSTALL_SCREEN_SERVICE_LOGS_TITLE);

  builder->Add("osInstallDialogSuccessTitle",
               IDS_OS_INSTALL_SCREEN_SUCCESS_TITLE);
  builder->Add("osInstallDialogSendFeedback",
               IDS_OS_INSTALL_SCREEN_SEND_FEEDBACK);
  builder->Add("osInstallDialogShutdownButton",
               IDS_OS_INSTALL_SCREEN_SHUTDOWN_BUTTON);
}

void OsInstallScreenHandler::Initialize() {}

void OsInstallScreenHandler::Show() {
  ShowScreen(kScreenId);
}

void OsInstallScreenHandler::Bind(ash::OsInstallScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
}

void OsInstallScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void OsInstallScreenHandler::ShowStep(const char* step) {
  CallJS("login.OsInstallScreen.showStep", std::string(step));
}

void OsInstallScreenHandler::ShowConfirmStep() {
  ShowStep(kConfirmStep);
}

void OsInstallScreenHandler::StartInstall() {
  ShowStep(kInProgressStep);

  OsInstallClient* const os_install_client = OsInstallClient::Get();

  os_install_client->AddObserver(this);
  os_install_client->StartOsInstall();
}

void OsInstallScreenHandler::StatusChanged(OsInstallClient::Status status,
                                           const std::string& service_log) {
  switch (status) {
    case OsInstallClient::Status::InProgress:
      ShowStep(kInProgressStep);
      break;
    case OsInstallClient::Status::Succeeded:
      ShowStep(kSuccessStep);
      break;
    case OsInstallClient::Status::Failed:
      ShowStep(kFailedStep);
      break;
    case OsInstallClient::Status::NoDestinationDeviceFound:
      ShowStep(kNoDestinationDeviceFoundStep);
      break;
  }
  CallJS("login.OsInstallScreen.setServiceLogs", service_log);
}

void OsInstallScreenHandler::OsInstallStarted(
    absl::optional<OsInstallClient::Status> status) {
  if (!status) {
    status = OsInstallClient::Status::Failed;
  }

  StatusChanged(*status, /*service_log=*/"");
}

}  // namespace chromeos
