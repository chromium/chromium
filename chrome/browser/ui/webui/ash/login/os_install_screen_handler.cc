// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/os_install_screen_handler.h"

#include <string>

#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/screens/os_install_screen.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {
namespace {
constexpr const char kInProgressStep[] = "in-progress";
constexpr const char kFailedStep[] = "failed";
constexpr const char kNoDestinationDeviceFoundStep[] =
    "no-destination-device-found";
constexpr const char kSuccessStep[] = "success";
}  // namespace

OsInstallScreenHandler::OsInstallScreenHandler()
    : BaseScreenHandler(kScreenId) {}

OsInstallScreenHandler::~OsInstallScreenHandler() = default;

void OsInstallScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->AddF("osInstallDialogIntroTitle", IDS_OS_INSTALL_SCREEN_INTRO_TITLE,
                IDS_INSTALLED_PRODUCT_OS_NAME);
  builder->AddF("osInstallDialogIntroSubtitle",
                IDS_OS_INSTALL_SCREEN_INTRO_SUBTITLE,
                IDS_INSTALLED_PRODUCT_OS_NAME);
  builder->AddF("osInstallDialogIntroBody0",
                IDS_OS_INSTALL_SCREEN_INTRO_CONTENT_0,
                IDS_INSTALLED_PRODUCT_OS_NAME);
  builder->Add("osInstallDialogIntroBody1",
               IDS_OS_INSTALL_SCREEN_INTRO_CONTENT_1);
  builder->AddF("osInstallDialogIntroFooter",
                IDS_OS_INSTALL_SCREEN_INTRO_FOOTER,
                IDS_INSTALLED_PRODUCT_OS_NAME);
  builder->AddF("osInstallDialogIntroNextButton",
                IDS_OS_INSTALL_SCREEN_INTRO_NEXT_BUTTON,
                IDS_INSTALLED_PRODUCT_OS_NAME);

  builder->AddF("osInstallDialogConfirmTitle",
                IDS_OS_INSTALL_SCREEN_CONFIRM_TITLE,
                IDS_INSTALLED_PRODUCT_OS_NAME);
  builder->Add("osInstallDialogConfirmBody",
               IDS_OS_INSTALL_SCREEN_CONFIRM_BODY);
  builder->Add("osInstallDialogConfirmNextButton",
               IDS_OS_INSTALL_SCREEN_CONFIRM_NEXT_BUTTON);

  builder->AddF("osInstallDialogInProgressTitle",
                IDS_OS_INSTALL_SCREEN_IN_PROGRESS_TITLE,
                IDS_INSTALLED_PRODUCT_OS_NAME);
  builder->Add("osInstallDialogInProgressSubtitle",
               IDS_OS_INSTALL_SCREEN_IN_PROGRESS_SUBTITLE);

  builder->Add("osInstallDialogErrorTitle", IDS_OS_INSTALL_SCREEN_ERROR_TITLE);
  builder->AddF("osInstallDialogErrorFailedSubtitle",
                IDS_OS_INSTALL_SCREEN_ERROR_FAILED_SUBTITLE,
                IDS_INSTALLED_PRODUCT_OS_NAME);
  builder->AddF("osInstallDialogErrorNoDestSubtitle",
                IDS_OS_INSTALL_SCREEN_ERROR_NO_DEST_SUBTITLE,
                IDS_INSTALLED_PRODUCT_OS_NAME);
  builder->Add("osInstallDialogErrorNoDestContent",
               IDS_OS_INSTALL_SCREEN_ERROR_NO_DEST_CONTENT);
  builder->Add("osInstallDialogServiceLogsTitle",
               IDS_OS_INSTALL_SCREEN_SERVICE_LOGS_TITLE);
  builder->Add("osInstallDialogErrorViewLogs",
               IDS_OS_INSTALL_SCREEN_ERROR_VIEW_LOGS);

  builder->Add("osInstallDialogSuccessTitle",
               IDS_OS_INSTALL_SCREEN_SUCCESS_TITLE);

  builder->Add("osInstallDialogSendFeedback",
               IDS_OS_INSTALL_SCREEN_SEND_FEEDBACK);
  builder->Add("osInstallDialogShutdownButton",
               IDS_OS_INSTALL_SCREEN_SHUTDOWN_BUTTON);
}

void OsInstallScreenHandler::Show() {
  ShowInWebUI();
}

void OsInstallScreenHandler::ShowStep(const char* step) {
  CallExternalAPI("showStep", std::string(step));
}

void OsInstallScreenHandler::SetStatus(OsInstallClient::Status status) {
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
}

void OsInstallScreenHandler::SetServiceLogs(const std::string& service_log) {
  CallExternalAPI("setServiceLogs", service_log);
}

void OsInstallScreenHandler::UpdateCountdownStringWithTime(
    base::TimeDelta time_left) {
  CallExternalAPI(
      "updateCountdownString",
      l10n_util::GetStringFUTF8(
          IDS_OS_INSTALL_SCREEN_SUCCESS_SUBTITLE,
          ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                                 ui::TimeFormat::LENGTH_LONG, time_left),
          l10n_util::GetStringUTF16(IDS_INSTALLED_PRODUCT_OS_NAME)));
}

base::WeakPtr<OsInstallScreenView> OsInstallScreenHandler::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace ash
