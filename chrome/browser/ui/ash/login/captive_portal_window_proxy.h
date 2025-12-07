// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_CAPTIVE_PORTAL_WINDOW_PROXY_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_CAPTIVE_PORTAL_WINDOW_PROXY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "ui/views/widget/widget_observer.h"

namespace content {
class WebContents;
}

namespace views {
class Widget;
}

namespace ash {
class CaptivePortalView;

// Proxy which manages showing of the window for CaptivePortal sign-in.
class CaptivePortalWindowProxy : public views::WidgetObserver {
 public:
  // Observer interface for CaptivePortalWindowProxy that gets notified when the
  // CaptivePortal widget is shown or hidden/closed.
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override = default;
    virtual void OnBeforeCaptivePortalShown() {}
    virtual void OnAfterCaptivePortalHidden() {}
  };

  explicit CaptivePortalWindowProxy(content::WebContents* web_contents);
  CaptivePortalWindowProxy(const CaptivePortalWindowProxy&) = delete;
  CaptivePortalWindowProxy& operator=(const CaptivePortalWindowProxy&) = delete;
  ~CaptivePortalWindowProxy() override;

  // Shows captive portal window only after a redirection has happened. So it is
  // safe to call this method, when the caller isn't 100% sure that the network
  // is in the captive portal state.
  // Subsequent call to this method would reuses existing view
  // but reloads test page (generate_204).
  void ShowIfRedirected(const std::string& network_name);

  // Forces captive portal window show.
  void Show(const std::string& network_name);

  // Closes the window.
  void Close();

  // Called by CaptivePortalView when URL loading was redirected from the
  // original URL.
  void OnRedirected(const std::string& network_name);

  // Called by CaptivePortalView when origin URL is loaded without any
  // redirections.
  void OnOriginalURLLoaded();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Overridden from views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override;

  bool IsDisplayedForTesting() const { return GetState() == STATE_DISPLAYED; }

 private:
  friend class CaptivePortalWindowTest;
  friend class SimpleWebViewDialogTest;

  // Possible transitions between states:
  //
  // wp(ShowIfRedirected(), WAITING_FOR_REDIRECTION) = IDLE
  // wp(Show(), DISPLAYED) = IDLE | WAITING_FOR_REDIRECTION
  // wp(Close(), IDLE) = WAITING_FOR_REDIRECTION | DISPLAYED
  // wp(OnRedirected(), DISPLAYED) = WAITING_FOR_REDIRECTION
  // wp(OnOriginalURLLoaded(), IDLE) = WAITING_FOR_REDIRECTION | DISPLAYED
  //
  // where wp(E, S) is a weakest precondition (initial state) such
  // that after execution of E the system will be surely in the state S.
  enum State {
    STATE_IDLE = 0,
    STATE_WAITING_FOR_REDIRECTION,
    STATE_DISPLAYED,
    STATE_UNKNOWN
  };

  // Initializes `captive_portal_view_` if it is not initialized and
  // starts loading Captive Portal redirect URL.
  void InitCaptivePortalView(const std::string& network_name);

  // Returns symbolic state name based on internal state.
  State GetState() const;

  // When `widget` is not NULL and the same as `widget_` stops to observe
  // notifications from `widget_` and resets it.
  void DetachFromWidget(views::Widget* widget);

  raw_ptr<Profile> profile_ = ProfileHelper::GetSigninProfile();
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<views::Widget> widget_ = nullptr;

  std::unique_ptr<CaptivePortalView> captive_portal_view_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<CaptivePortalWindowProxy> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_CAPTIVE_PORTAL_WINDOW_PROXY_H_
