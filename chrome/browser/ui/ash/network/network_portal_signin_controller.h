// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_SIGNIN_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_SIGNIN_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "ui/views/widget/widget_observer.h"
#include "url/gurl.h"

class Profile;

namespace ash {

class NetworkPortalSigninController : public views::WidgetObserver,
                                      public NetworkStateHandlerObserver {
 public:
  // Keep this in sync with the NetworkPortalSigninMode enum in
  // tools/metrics/histograms/metadata/network/enums.xml.
  enum class SigninMode {
    // Show in a dialog window during oobe/login.
    kSigninDialog = 1,
    // kSingletonTab (2) was deprecated in M110
    // Show in a new tab using the active user profile (proxies enabled).
    kNormalTab = 3,
    // Default mode. Proxies will be disabled for captive portal signin.
    kSigninDefault = 4,
    // DEPRECATED: kIncognitoDialog = 5,
    // Incognito mode is disabled by policy.
    kIncognitoDisabledByPolicy = 6,
    // Incognito mode is disabled by parental controls.
    kIncognitoDisabledByParentalControls = 7,
    kMaxValue = 7,
  };
  friend std::ostream& operator<<(std::ostream& stream,
                                  const SigninMode& signin_mode);

  // Keep this in sync with the NetworkPortalSigninSource enum in
  // tools/metrics/histograms/metadata/network/enums.xml.
  enum class SigninSource {
    // Opened from a notification.
    kNotification = 1,
    // Opened from the Settings UI.
    kSettings = 2,
    // Opened from the QuickSettings UI.
    kQuickSettings = 3,
    // Opened from the Chrome error page.
    kErrorPage = 4,
    kMaxValue = 4,
  };
  friend std::ostream& operator<<(std::ostream& stream,
                                  const SigninSource& signin_mode);

  static NetworkPortalSigninController* Get();

  NetworkPortalSigninController(const NetworkPortalSigninController&) = delete;
  NetworkPortalSigninController& operator=(
      const NetworkPortalSigninController&) = delete;
  ~NetworkPortalSigninController() override;

  // Shows the signin UI.
  void ShowSignin(SigninSource source);

  // Closes the signin UI if appropriate.
  void CloseSignin();

  // Returns whether the sigin UI is show.
  bool DialogIsShown();

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // NetworkStateHandlerObserver:
  void PortalStateChanged(const NetworkState* default_network,
                          NetworkState::PortalState portal_state) override;
  void OnShuttingDown() override;

 protected:
  friend class base::NoDestructor<NetworkPortalSigninController>;
  friend class NetworkPortalSigninControllerTest;

  NetworkPortalSigninController();

  // Shows the signin UI in a dialog window using the 'signin' (login) profile.
  // Overridden in tests.
  virtual void ShowSigninDialog(const GURL& url);

  // Shows the signin UI in a NetworkPortalSigninWindow window which uses a
  // dedicated OTR profile. Overridden in tests.
  virtual void ShowSigninWindow(const GURL& url);

  // Shows the signin UI in browser tab using the specified profile. Overridden
  // in tests.
  virtual void ShowTab(Profile* profile, const GURL& url);

  // Shows the signin UI in a browser tab using the active user profile.
  // Overridden in tests.
  virtual void ShowActiveProfileTab(const GURL& url);

  SigninMode GetSigninMode(NetworkState::PortalState portal_state) const;

 private:
  raw_ptr<views::Widget> dialog_widget_ = nullptr;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      dialog_widget_observation_{this};
  NetworkStateHandlerScopedObservation network_state_handler_observation_{this};
  std::string signin_network_guid_;
  base::TimeTicks signin_start_time_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_SIGNIN_CONTROLLER_H_
