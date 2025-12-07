// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_TEST_MOCK_BROWSER_WINDOW_INTERFACE_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_TEST_MOCK_BROWSER_WINDOW_INTERFACE_H_

#include "base/callback_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockBrowserWindowInterface : public BrowserWindowInterface {
 public:
  MockBrowserWindowInterface();
  ~MockBrowserWindowInterface() override;

  MOCK_METHOD(Profile*, GetProfile, (), (override));
  MOCK_METHOD(const Profile*, GetProfile, (), (const override));
  MOCK_METHOD(void,
              OpenGURL,
              (const GURL& gurl, WindowOpenDisposition disposition),
              (override));
  MOCK_METHOD(const SessionID&, GetSessionID, (), (const override));
  MOCK_METHOD(TabStripModel*, GetTabStripModel, (), (override));
  MOCK_METHOD(const TabStripModel*, GetTabStripModel, (), (const, override));
  MOCK_METHOD(bool, IsTabStripVisible, (), (override));
  MOCK_METHOD(bool, ShouldHideUIForFullscreen, (), (const, override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterBrowserDidClose,
              (BrowserDidCloseCallback callback),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterBrowserCloseCancelled,
              (BrowserCloseCancelledCallback callback),
              (override));
  MOCK_METHOD(base::WeakPtr<BrowserWindowInterface>,
              GetWeakPtr,
              (),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterActiveTabDidChange,
              (ActiveTabChangeCallback callback),
              (override));
  MOCK_METHOD(tabs::TabInterface*, GetActiveTabInterface, (), (override));
  MOCK_METHOD(BrowserWindowFeatures&, GetFeatures, (), (override));
  MOCK_METHOD(const BrowserWindowFeatures&, GetFeatures, (), (const, override));
  // The non-const version should never return something different from the
  // const version, so implement one in terms of th other.
  ui::UnownedUserDataHost& GetUnownedUserDataHost() override;
  MOCK_METHOD(const ui::UnownedUserDataHost&,
              GetUnownedUserDataHost,
              (),
              (const, override));
  MOCK_METHOD(web_modal::WebContentsModalDialogHost*,
              GetWebContentsModalDialogHostForWindow,
              (),
              (override));
  MOCK_METHOD(web_modal::WebContentsModalDialogHost*,
              GetWebContentsModalDialogHostForTab,
              (tabs::TabInterface * tab_interface),
              (override));
  MOCK_METHOD(bool, IsActive, (), (const, override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterDidBecomeActive,
              (DidBecomeActiveCallback callback),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterDidBecomeInactive,
              (DidBecomeInactiveCallback callback),
              (override));
  MOCK_METHOD(ExclusiveAccessManager*,
              GetExclusiveAccessManager,
              (),
              (override));
  MOCK_METHOD(BrowserActions*, GetActions, (), (override));
  MOCK_METHOD(Type, GetType, (), (const, override));
  MOCK_METHOD(std::vector<tabs::TabInterface*>,
              GetAllTabInterfaces,
              (),
              (override));
  MOCK_METHOD(Browser*, GetBrowserForMigrationOnly, (), (override));
  MOCK_METHOD(const Browser*,
              GetBrowserForMigrationOnly,
              (),
              (const, override));
  MOCK_METHOD(bool, IsTabModalPopupDeprecated, (), (const, override));
  MOCK_METHOD(ui::BaseWindow*, GetWindow, (), (override));
  MOCK_METHOD(const ui::BaseWindow*, GetWindow, (), (const override));
  MOCK_METHOD(DesktopBrowserWindowCapabilities*, capabilities, (), (override));
  MOCK_METHOD(const DesktopBrowserWindowCapabilities*,
              capabilities,
              (),
              (const, override));

  // PageNavigator methods
  MOCK_METHOD(content::WebContents*,
              OpenURL,
              (const content::OpenURLParams& params,
               base::OnceCallback<void(content::NavigationHandle&)>
                   navigation_handle_callback),
              (override));

  MOCK_METHOD(bool, CanShowCallToAction, (), (const, override));
  MOCK_METHOD(std::unique_ptr<ScopedWindowCallToAction>,
              ShowCallToAction,
              (),
              (override));
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_TEST_MOCK_BROWSER_WINDOW_INTERFACE_H_
