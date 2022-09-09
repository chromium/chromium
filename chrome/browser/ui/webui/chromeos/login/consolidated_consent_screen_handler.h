// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stub handler

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_CONSOLIDATED_CONSENT_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_CONSOLIDATED_CONSENT_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class ConsolidatedConsentScreen;
}

namespace chromeos {
namespace {
const char kGoogleEulaDefaultUrl[] =
    "https://policies.google.com/terms/embedded?hl=en";
const char kCrosEulaDefaultUrl[] =
    "https://www.google.com/intl/en/chrome/terms/";
}  // namespace

// Interface for dependency injection between ConsolidatedConsentScreen and its
// WebUI representation.
class ConsolidatedConsentScreenView
    : public base::SupportsWeakPtr<ConsolidatedConsentScreenView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "consolidated-consent", "ConsolidatedConsentScreen"};

  struct ScreenConfig {
    ScreenConfig();
    ~ScreenConfig();
    ScreenConfig(const ScreenConfig&) = delete;
    ScreenConfig& operator=(const ScreenConfig&) = delete;

    bool is_arc_enabled = true;
    bool is_demo = false;
    bool is_tos_hidden = false;
    bool is_child_account = false;
    std::string country_code = "us";

    // Default URLs with english locales.
    // ScreenConfig created in ConsolidatedConsentScreen::ShowImpl() should
    // include the localized versions.
    std::string google_eula_url = kGoogleEulaDefaultUrl;
    std::string cros_eula_url = kCrosEulaDefaultUrl;
  };

  virtual ~ConsolidatedConsentScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show(base::Value::Dict data) = 0;

  // Updates the UI of the opt-ins.
  // When an opt-in is managed, its toggle would be disabled.
  virtual void SetUsageMode(bool enabled, bool managed) = 0;
  virtual void SetBackupMode(bool enabled, bool managed) = 0;
  virtual void SetLocationMode(bool enabled, bool managed) = 0;

  // Set the visibility of the usage opt-in. For non-demo scenarios, the screen
  // will stay in the `loading` step until this method is called.
  virtual void SetUsageOptinOptinHidden(bool hidden) = 0;
};

class ConsolidatedConsentScreenHandler : public ConsolidatedConsentScreenView,
                                         public BaseScreenHandler {
 public:
  using TView = ConsolidatedConsentScreenView;

  ConsolidatedConsentScreenHandler();

  ~ConsolidatedConsentScreenHandler() override;

  ConsolidatedConsentScreenHandler(const ConsolidatedConsentScreenHandler&) =
      delete;
  ConsolidatedConsentScreenHandler& operator=(
      const ConsolidatedConsentScreenHandler&) = delete;

 private:
  // ConsolidatedConsentScreenView
  void Show(base::Value::Dict data) override;
  void SetUsageMode(bool enabled, bool managed) override;
  void SetBackupMode(bool enabled, bool managed) override;
  void SetLocationMode(bool enabled, bool managed) override;
  void SetUsageOptinOptinHidden(bool hidden) override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::ConsolidatedConsentScreenHandler;
using ::chromeos::ConsolidatedConsentScreenView;
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_CONSOLIDATED_CONSENT_SCREEN_HANDLER_H_
