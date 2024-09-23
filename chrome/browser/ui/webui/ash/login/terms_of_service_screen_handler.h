// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_TERMS_OF_SERVICE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_TERMS_OF_SERVICE_SCREEN_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface for dependency injection between TermsOfServiceScreen and its
// WebUI representation.
class TermsOfServiceScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"terms-of-service",
                                                       "TermsOfServiceScreen"};

  virtual ~TermsOfServiceScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show(const std::string& manager) = 0;

  // Called when the download of the Terms of Service fails. Show an error
  // message to the user.
  virtual void OnLoadError() = 0;

  // Called when the download of the Terms of Service is successful. Shows the
  // downloaded `terms_of_service` to the user.
  virtual void OnLoadSuccess(const std::string& terms_of_service) = 0;

  // Whether TOS are successfully loaded.
  virtual bool AreTermsLoaded() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<TermsOfServiceScreenView> AsWeakPtr() = 0;
};

// The sole implementation of the TermsOfServiceScreenView, using WebUI.
class TermsOfServiceScreenHandler final : public BaseScreenHandler,
                                          public TermsOfServiceScreenView {
 public:
  using TView = TermsOfServiceScreenView;

  TermsOfServiceScreenHandler();

  TermsOfServiceScreenHandler(const TermsOfServiceScreenHandler&) = delete;
  TermsOfServiceScreenHandler& operator=(const TermsOfServiceScreenHandler&) =
      delete;

  ~TermsOfServiceScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // TermsOfServiceScreenView:
  void Show(const std::string& manager) override;
  void OnLoadError() override;
  void OnLoadSuccess(const std::string& terms_of_service) override;
  bool AreTermsLoaded() override;
  base::WeakPtr<TermsOfServiceScreenView> AsWeakPtr() override;

 private:
  // Set to `true` when the download of the Terms of Service succeeds.
  bool terms_loaded_ = false;

  base::WeakPtrFactory<TermsOfServiceScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_TERMS_OF_SERVICE_SCREEN_HANDLER_H_
