// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/kiosk_enable_screen_handler.h"

#include <string>

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/screens/kiosk_enable_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {

constexpr StaticOobeScreenId KioskEnableScreenView::kScreenId;

KioskEnableScreenHandler::KioskEnableScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {}

KioskEnableScreenHandler::~KioskEnableScreenHandler() {
  if (delegate_)
    delegate_->OnViewDestroyed(this);
}

void KioskEnableScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  KioskAppManager::Get()->GetConsumerKioskAutoLaunchStatus(
      base::Bind(
          &KioskEnableScreenHandler::OnGetConsumerKioskAutoLaunchStatus,
          weak_ptr_factory_.GetWeakPtr()));
}

void KioskEnableScreenHandler::OnGetConsumerKioskAutoLaunchStatus(
    KioskAppManager::ConsumerKioskAutoLaunchStatus status) {
  is_configurable_ =
      (status == KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_CONFIGURABLE);
  if (!is_configurable_) {
    LOG(WARNING) << "Consumer kiosk auto launch feature is not configurable!";
    return;
  }

  ShowScreen(kScreenId);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_KIOSK_ENABLE_WARNING_VISIBLE,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

void KioskEnableScreenHandler::SetDelegate(KioskEnableScreen* delegate) {
  delegate_ = delegate;
  if (page_is_ready())
    Initialize();
}

void KioskEnableScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("kioskEnableWarningText",
               IDS_KIOSK_ENABLE_SCREEN_WARNING);
  builder->Add("kioskEnableWarningDetails",
               IDS_KIOSK_ENABLE_SCREEN_WARNING_DETAILS);
  builder->Add("kioskEnableButton", IDS_KIOSK_ENABLE_SCREEN_ENABLE_BUTTON);
  builder->Add("kioskCancelButton", IDS_CANCEL);
  builder->Add("kioskOKButton", IDS_OK);
  builder->Add("kioskEnableSuccessMsg", IDS_KIOSK_ENABLE_SCREEN_SUCCESS);
  builder->Add("kioskEnableErrorMsg", IDS_KIOSK_ENABLE_SCREEN_ERROR);
}

void KioskEnableScreenHandler::Initialize() {
  if (!page_is_ready() || !delegate_)
    return;

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void KioskEnableScreenHandler::RegisterMessages() {
  AddCallback("kioskOnClose", &KioskEnableScreenHandler::HandleOnClose);
  AddCallback("kioskOnEnable", &KioskEnableScreenHandler::HandleOnEnable);
}

void KioskEnableScreenHandler::HandleOnClose() {
  if (delegate_)
    delegate_->OnExit();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_KIOSK_ENABLE_WARNING_COMPLETED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

void KioskEnableScreenHandler::HandleOnEnable() {
  if (!is_configurable_) {
    NOTREACHED();
    if (delegate_)
      delegate_->OnExit();

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_KIOSK_ENABLE_WARNING_COMPLETED,
        content::NotificationService::AllSources(),
        content::NotificationService::NoDetails());
    return;
  }

  KioskAppManager::Get()->EnableConsumerKioskAutoLaunch(
      base::Bind(&KioskEnableScreenHandler::OnEnableConsumerKioskAutoLaunch,
                 weak_ptr_factory_.GetWeakPtr()));
}

void KioskEnableScreenHandler::OnEnableConsumerKioskAutoLaunch(
    bool success) {
  if (!success)
    LOG(WARNING) << "Consumer kiosk mode can't be enabled!";

  CallJS("login.KioskEnableScreen.onCompleted", success);
  if (success) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_KIOSK_ENABLED,
        content::NotificationService::AllSources(),
        content::NotificationService::NoDetails());
  }
}

}  // namespace chromeos
