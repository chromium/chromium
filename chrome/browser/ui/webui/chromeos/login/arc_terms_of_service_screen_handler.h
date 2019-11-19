// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ARC_TERMS_OF_SERVICE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ARC_TERMS_OF_SERVICE_SCREEN_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/arc/optin/arc_optin_preference_handler_observer.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/settings/timezone_settings.h"

namespace arc {
class ArcOptInPreferenceHandler;
}

namespace chromeos {

class ArcTermsOfServiceScreen;
class ArcTermsOfServiceScreenView;

class ArcTermsOfServiceScreenViewObserver {
 public:
  virtual ~ArcTermsOfServiceScreenViewObserver() = default;

  // Called when the user skips the PlayStore Terms of Service.
  virtual void OnSkip() = 0;

  // Called when the user accepts the PlayStore Terms of Service.
  virtual void OnAccept(bool review_arc_settings) = 0;

  // Called when the view is destroyed so there is no dead reference to it.
  virtual void OnViewDestroyed(ArcTermsOfServiceScreenView* view) = 0;

 protected:
  ArcTermsOfServiceScreenViewObserver() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcTermsOfServiceScreenViewObserver);
};

class ArcTermsOfServiceScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"arc-tos"};

  virtual ~ArcTermsOfServiceScreenView() = default;

  // Adds/Removes observer for view.
  virtual void AddObserver(ArcTermsOfServiceScreenViewObserver* observer) = 0;
  virtual void RemoveObserver(
      ArcTermsOfServiceScreenViewObserver* observer) = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Sets view and screen.
  virtual void Bind(ArcTermsOfServiceScreen* screen) = 0;

 protected:
  ArcTermsOfServiceScreenView() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcTermsOfServiceScreenView);
};

// The sole implementation of the ArcTermsOfServiceScreenView, using WebUI.
class ArcTermsOfServiceScreenHandler
    : public BaseScreenHandler,
      public ArcTermsOfServiceScreenView,
      public arc::ArcOptInPreferenceHandlerObserver,
      public OobeUI::Observer,
      public system::TimezoneSettings::Observer,
      public chromeos::NetworkStateHandlerObserver {
 public:
  using TView = ArcTermsOfServiceScreenView;

  explicit ArcTermsOfServiceScreenHandler(JSCallsContainer* js_calls_container);
  ~ArcTermsOfServiceScreenHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // ArcTermsOfServiceScreenView:
  void AddObserver(ArcTermsOfServiceScreenViewObserver* observer) override;
  void RemoveObserver(ArcTermsOfServiceScreenViewObserver* observer) override;
  void Show() override;
  void Hide() override;
  void Bind(ArcTermsOfServiceScreen* screen) override;

  // OobeUI::Observer:
  void OnCurrentScreenChanged(OobeScreenId current_screen,
                              OobeScreenId new_screen) override;
  void OnDestroyingOobeUI() override {}

  // system::TimezoneSettings::Observer:
  void TimezoneChanged(const icu::TimeZone& timezone) override;

  // chromeos::NetworkStateHandlerObserver:
  void DefaultNetworkChanged(const NetworkState* network) override;

 private:
  // BaseScreenHandler:
  void Initialize() override;

  // Shows default terms of service screen.
  void DoShow();

  // Shows screen variant for demo mode setup flow. The flow is part of OOBE and
  // runs before any user is created or before device local account is
  // configured for Public Session.
  void DoShowForDemoModeSetup();

  void HandleSkip(const std::string& tos_content);
  void HandleAccept(bool enable_backup_restore,
                    bool enable_location_services,
                    bool review_arc_settings,
                    const std::string& tos_content);
  // Loads Play Store ToS content in case default network exists. If
  // |ignore_network_state| is set then network state is not checked.
  void MaybeLoadPlayStoreToS(bool ignore_network_state);

  void StartNetworkAndTimeZoneObserving();

  // Handles the recording of consent given or not given after the user chooses
  // to skip or accept.
  void RecordConsents(const std::string& tos_content,
                      bool record_tos_content,
                      bool tos_accepted,
                      bool record_backup_consent,
                      bool backup_accepted,
                      bool record_location_consent,
                      bool location_accepted);

  bool NeedDispatchEventOnAction();

  // arc::ArcOptInPreferenceHandlerObserver:
  void OnMetricsModeChanged(bool enabled, bool managed) override;
  void OnBackupAndRestoreModeChanged(bool enabled, bool managed) override;
  void OnLocationServicesModeChanged(bool enabled, bool managed) override;

  base::ObserverList<ArcTermsOfServiceScreenViewObserver, true>::Unchecked
      observer_list_;

  // Whether the screen should be shown right after initialization.
  bool show_on_init_ = false;

  // Indicates that we already started network and time zone observing.
  bool network_time_zone_observing_ = false;

  // To filter out duplicate notifications from html.
  bool action_taken_ = false;

  // To track if ARC preference is managed.
  bool arc_managed_ = false;

  // To track if optional features are managed preferences.
  bool backup_restore_managed_ = false;
  bool location_services_managed_ = false;

  // To track if a child account is being set up.
  bool is_child_account_;

  std::unique_ptr<arc::ArcOptInPreferenceHandler> pref_handler_;

  DISALLOW_COPY_AND_ASSIGN(ArcTermsOfServiceScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ARC_TERMS_OF_SERVICE_SCREEN_HANDLER_H_
