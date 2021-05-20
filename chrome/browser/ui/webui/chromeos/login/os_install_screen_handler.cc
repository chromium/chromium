// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/os_install_screen_handler.h"

#include "chrome/browser/ash/login/screens/os_install_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/js_calls_container.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

// static
constexpr StaticOobeScreenId OsInstallScreenView::kScreenId;

OsInstallScreenHandler::OsInstallScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {}

OsInstallScreenHandler::~OsInstallScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void OsInstallScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("osInstallDialogIntroTitle", IDS_OS_INSTALL_SCREEN_INTRO_TITLE);
}

void OsInstallScreenHandler::Initialize() {}

void OsInstallScreenHandler::RegisterMessages() {}

void OsInstallScreenHandler::Show() {
  ShowScreen(kScreenId);
}

void OsInstallScreenHandler::Bind(OsInstallScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
}

void OsInstallScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

}  // namespace chromeos
