// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_SIGNIN_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_SIGNIN_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "ui/views/widget/widget_observer.h"
#include "url/gurl.h"

class Profile;

namespace ash {

class NetworkPortalSigninController : public views::WidgetObserver,
                                      public NetworkStateHandlerObserver {
 public:
  // Keep this in sync with the NetworkPortalSigninMode enum in
  // tools/metrics/histograms/enums.xml.
  enum class SigninMode {
    // Show in a dialog window using the signin (oobe/login) profile.
    kSigninDialog = 1,
    // kSingletonTab (2) was deprecated in M110
    // Show in a new tab using the active user profile.
    kNormalTab = 3,
    // Show in a new tab in an OTR window with the portal signin profile.
    kIncognitoTab = 4,
    // DEPRECATED: kIncognitoDialog = 5,
    // Show in a dialog window using the portal signin profile due to Incognito
    // browsing disabled.
    kIncognitoDialogDisabled = 6,
    // Show in a dialog window using the portal signin profile due to parential
    // controls disabling incognito browsing.
    kIncognitoDialogParental = 7,
    kMaxValue = 7,
  };
  friend std::ostream& operator<<(std::ostream& stream,
                                  const SigninMode& signin_mode);

  // Keep this in sync with the NetworkPortalSigninSource enum in
  // tools/metrics/histograms/enums.xml.
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

 protected:
  friend class base::NoDestructor<NetworkPortalSigninController>;
  NetworkPortalSigninController();

  // May be overridden in tests.
  virtual void ShowDialog(Profile* profile, const GURL& url);
  virtual void ShowTab(Profile* profile, const GURL& url);

  SigninMode GetSigninMode() const;

 private:
  raw_ptr<views::Widget> dialog_widget_ = nullptr;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      dialog_widget_observation_{this};
  NetworkStateHandlerScopedObservation network_state_handler_observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_SIGNIN_CONTROLLER_H_
