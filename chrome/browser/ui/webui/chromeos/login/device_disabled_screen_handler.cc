// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/device_disabled_screen_handler.h"

#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/device_disabled_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

constexpr StaticOobeScreenId DeviceDisabledScreenView::kScreenId;

DeviceDisabledScreenHandler::DeviceDisabledScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
}

DeviceDisabledScreenHandler::~DeviceDisabledScreenHandler() {
  if (delegate_)
    delegate_->OnViewDestroyed(this);
}

void DeviceDisabledScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  if (delegate_) {
    CallJS("login.DeviceDisabledScreen.setSerialNumberAndEnrollmentDomain",
           delegate_->GetSerialNumber(), delegate_->GetEnrollmentDomain());
    CallJS("login.DeviceDisabledScreen.setMessage", delegate_->GetMessage());
  }
  ShowScreen(kScreenId);
}

void DeviceDisabledScreenHandler::Hide() {
  show_on_init_ = false;
}

void DeviceDisabledScreenHandler::SetDelegate(DeviceDisabledScreen* delegate) {
  delegate_ = delegate;
  if (page_is_ready())
    Initialize();
}

void DeviceDisabledScreenHandler::UpdateMessage(const std::string& message) {
  if (page_is_ready())
    CallJS("login.DeviceDisabledScreen.setMessage", message);
}

void DeviceDisabledScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("deviceDisabledHeading", IDS_DEVICE_DISABLED_HEADING);
  builder->Add("deviceDisabledExplanationWithDomain",
               IDS_DEVICE_DISABLED_EXPLANATION_WITH_DOMAIN);
  builder->Add("deviceDisabledExplanationWithoutDomain",
               IDS_DEVICE_DISABLED_EXPLANATION_WITHOUT_DOMAIN);
}

void DeviceDisabledScreenHandler::Initialize() {
  if (!page_is_ready() || !delegate_)
    return;

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void DeviceDisabledScreenHandler::RegisterMessages() {
}

}  // namespace chromeos
