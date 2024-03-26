// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/base_webui_handler.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "components/login/localized_values_builder.h"
#include "content/public/browser/web_ui.h"

namespace ash {

BaseWebUIHandler::BaseWebUIHandler() = default;

BaseWebUIHandler::~BaseWebUIHandler() = default;

void BaseWebUIHandler::OnJavascriptAllowed() {
  auto deferred_calls = std::exchange(deferred_calls_, {});
  for (auto& call : deferred_calls)
    std::move(call).Run();

  InitAfterJavascriptAllowed();
}

void BaseWebUIHandler::GetLocalizedStrings(base::Value::Dict* dict) {
  auto builder = std::make_unique<::login::LocalizedValuesBuilder>(dict);
  DeclareLocalizedValues(builder.get());
  GetAdditionalParameters(dict);
}

void BaseWebUIHandler::RegisterMessages() {
  DeclareJSCallbacks();
}

void BaseWebUIHandler::GetAdditionalParameters(base::Value::Dict* dict) {}

void BaseWebUIHandler::InitAfterJavascriptAllowed() {}

void BaseWebUIHandler::ShowScreenDeprecated(OobeScreenId screen) {
  if (!GetOobeUI())
    return;
  GetOobeUI()->GetCoreOobe()->ShowScreenWithData(screen, std::nullopt);
}

OobeUI* BaseWebUIHandler::GetOobeUI() {
  return static_cast<OobeUI*>(web_ui()->GetController());
}

OobeScreenId BaseWebUIHandler::GetCurrentScreen() {
  OobeUI* oobe_ui = GetOobeUI();
  if (!oobe_ui)
    return OOBE_SCREEN_UNKNOWN;
  return oobe_ui->current_screen();
}

}  // namespace ash
