// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SUPPORT_TOOL_SUPPORT_TOOL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SUPPORT_TOOL_SUPPORT_TOOL_UI_H_

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

// This enum is emitted to UMA
// `Browser.SupportTool.SupportToolWebUIAction` and can't be
// renumerated. Please update `SupportToolWebUIActionType` values
// in tools/metrics/histograms/enums.xml when you add a new value here.
enum class SupportToolWebUIActionType {
  // When the user opens Support Tool page.
  kOpenSupportToolPage = 0,
  // When the user opens URL generator page.
  kOpenURLGeneratorPage = 1,
  // When the support packet is created and added to the selected file path.
  kCreateSupportPacket = 2,
  // When user cancels data collection and starts from the beginning.
  kCancelDataCollection = 3,
  // When user generates URL on chrome://support-tool/url-generator.
  kGenerateURL = 4,
  // When user generates support token on chrome://support-tool/url-generator.
  kGenerateToken = 5,
  kMaxValue = kGenerateToken,
};

// The histogram name for the UMA metrics we use for recording the WebUI
// actions.
extern const char kSupportToolWebUIActionHistogram[];

class SupportToolUI;

class SupportToolUIConfig : public content::DefaultWebUIConfig<SupportToolUI> {
 public:
  SupportToolUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISupportToolHost) {}

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The C++ back-end for the chrome://support-tool webui page.
class SupportToolUI : public content::WebUIController {
 public:
  explicit SupportToolUI(content::WebUI* web_ui);

  SupportToolUI(const SupportToolUI&) = delete;
  SupportToolUI& operator=(const SupportToolUI&) = delete;

  ~SupportToolUI() override;

  static base::Value::Dict GetLocalizedStrings();

  // Returns if Support Tool should be enabled. Support Tool is only
  // enabled in managed devices and logged-in sessions on consumer devices. It
  // won't be available in guest sessions of consumer-owned devices.
  static bool IsEnabled(Profile* profile);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SUPPORT_TOOL_SUPPORT_TOOL_UI_H_
