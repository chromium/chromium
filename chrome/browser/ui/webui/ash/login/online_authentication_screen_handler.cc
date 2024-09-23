// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/online_authentication_screen_handler.h"

#include "base/logging.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/online_authentication_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

OnlineAuthenticationScreenHandler::OnlineAuthenticationScreenHandler()
    : BaseScreenHandler(kScreenId) {}

OnlineAuthenticationScreenHandler::~OnlineAuthenticationScreenHandler() =
    default;

void OnlineAuthenticationScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

void OnlineAuthenticationScreenHandler::DeclareJSCallbacks() {}

void OnlineAuthenticationScreenHandler::Show() {
  ShowInWebUI();
}

void OnlineAuthenticationScreenHandler::Hide() {}

base::WeakPtr<OnlineAuthenticationScreenView>
OnlineAuthenticationScreenHandler::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace ash
