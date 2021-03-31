// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_TERMS_OF_SERVICE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_TERMS_OF_SERVICE_SCREEN_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class TermsOfServiceScreen;

// Interface for dependency injection between TermsOfServiceScreen and its
// WebUI representation.
class TermsOfServiceScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"terms-of-service"};

  virtual ~TermsOfServiceScreenView() {}

  // Sets screen this view belongs to.
  virtual void SetScreen(TermsOfServiceScreen* screen) = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Sets the manager whose Terms of Service are being shown.
  virtual void SetManager(const std::string& manager) = 0;

  // Called when the download of the Terms of Service fails. Show an error
  // message to the user.
  virtual void OnLoadError() = 0;

  // Called when the download of the Terms of Service is successful. Shows the
  // downloaded `terms_of_service` to the user.
  virtual void OnLoadSuccess(const std::string& terms_of_service) = 0;

  // Whether TOS are successfully loaded.
  virtual bool AreTermsLoaded() = 0;
};

// The sole implementation of the TermsOfServiceScreenView, using WebUI.
class TermsOfServiceScreenHandler : public BaseScreenHandler,
                                    public TermsOfServiceScreenView {
 public:
  using TView = TermsOfServiceScreenView;

  explicit TermsOfServiceScreenHandler(JSCallsContainer* js_calls_container);
  ~TermsOfServiceScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // TermsOfServiceScreenView:
  void SetScreen(TermsOfServiceScreen* screen) override;
  void Show() override;
  void Hide() override;
  void SetManager(const std::string& manager) override;
  void OnLoadError() override;
  void OnLoadSuccess(const std::string& terms_of_service) override;
  bool AreTermsLoaded() override;

 private:
  // BaseScreenHandler:
  void Initialize() override;

  // Switch to the user's preferred input method and show the screen. This
  // method is called after it has been ensured that the current UI locale
  // matches the UI locale chosen by the user.
  void DoShow();

  // Update the manager shown in the UI.
  void UpdateManagerInUI();

  // Update the UI to show an error message or the Terms of Service, depending
  // on whether the download of the Terms of Service was successful. Does
  // nothing if the download is still in progress.
  void UpdateTermsOfServiceInUI();

  TermsOfServiceScreen* screen_ = nullptr;

  // Whether the screen should be shown right after initialization.
  bool show_on_init_ = false;

  // The manager whose Terms of Service are being shown.
  std::string manager_;

  // Set to `true` when the download of the Terms of Service fails.
  bool load_error_ = false;

  // Set to the Terms of Service when the download is successful.
  std::string terms_of_service_;

  DISALLOW_COPY_AND_ASSIGN(TermsOfServiceScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_TERMS_OF_SERVICE_SCREEN_HANDLER_H_
