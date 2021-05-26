// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_TELEMETRY_EXTENSION_UNTRUSTED_UI_H_
#define CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_TELEMETRY_EXTENSION_UNTRUSTED_UI_H_

#include <map>
#include <memory>
#include <string>

#include "base/strings/string_piece.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/webui/untrusted_web_ui_controller.h"
#include "ui/webui/webui_config.h"
#include "url/gurl.h"

namespace chromeos {

class TelemetryExtensionUntrustedUIConfig : public ui::WebUIConfig {
 public:
  TelemetryExtensionUntrustedUIConfig();
  ~TelemetryExtensionUntrustedUIConfig() override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui) override;
};

class TelemetryExtensionUntrustedUI : public ui::UntrustedWebUIController {
 public:
  explicit TelemetryExtensionUntrustedUI(content::WebUI* web_ui);
  TelemetryExtensionUntrustedUI(const TelemetryExtensionUntrustedUI&) = delete;
  TelemetryExtensionUntrustedUI& operator=(
      const TelemetryExtensionUntrustedUI&) = delete;
  ~TelemetryExtensionUntrustedUI() override;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_TELEMETRY_EXTENSION_UNTRUSTED_UI_H_
