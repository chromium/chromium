// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/app_icon_webui_handler.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_analysis.h"

namespace {

base::Value GetDominantColorCssString(const SkBitmap& bitmap) {
  color_utils::GridSampler sampler;
  SkColor color = color_utils::CalculateKMeanColorOfBitmap(bitmap);
  return base::Value(base::StringPrintf("rgb(%d, %d, %d)", SkColorGetR(color),
                                        SkColorGetG(color),
                                        SkColorGetB(color)));
}

}  // namespace

AppIconWebUIHandler::AppIconWebUIHandler() {
  extension_icon_manager_.set_observer(this);
}

AppIconWebUIHandler::~AppIconWebUIHandler() = default;

void AppIconWebUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getAppIconDominantColor",
      base::BindRepeating(&AppIconWebUIHandler::HandleGetAppIconDominantColor,
                          base::Unretained(this)));
}

void AppIconWebUIHandler::HandleGetAppIconDominantColor(
    const base::Value::List& args) {
  const std::string& extension_id = args[0].GetString();

  Profile* profile = Profile::FromWebUI(web_ui());
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* extension =
      extension_registry->enabled_extensions().GetByID(extension_id);
  if (!extension)
    return;
  extension_icon_manager_.LoadIcon(profile, extension);
}

void AppIconWebUIHandler::OnImageLoaded(const std::string& extension_id) {
  gfx::Image icon = extension_icon_manager_.GetIcon(extension_id);
  base::Value color_value = GetDominantColorCssString(*icon.ToSkBitmap());
  base::Value id(extension_id);
  web_ui()->CallJavascriptFunctionUnsafe("ntp.setFaviconDominantColor", id,
                                         color_value);
}
