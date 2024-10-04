// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_TEST_MOCK_BROWSER_WINDOW_INTERFACE_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_TEST_MOCK_BROWSER_WINDOW_INTERFACE_H_

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockBrowserWindowInterface : public BrowserWindowInterface {
 public:
  MockBrowserWindowInterface();
  ~MockBrowserWindowInterface() override;

  MOCK_METHOD(views::WebView*, GetWebView, (), (override));
  MOCK_METHOD(Profile*, GetProfile, (), (override));
  MOCK_METHOD(void,
              OpenGURL,
              (const GURL& gurl, WindowOpenDisposition disposition),
              (override));
  MOCK_METHOD(const SessionID&, GetSessionID, (), (override));
  MOCK_METHOD(TabStripModel*, GetTabStripModel, (), (override));
  MOCK_METHOD(bool, IsTabStripVisible, (), (override));
  MOCK_METHOD(bool, ShouldHideUIForFullscreen, (), (const, override));
  MOCK_METHOD(bool, IsAttemptingToCloseBrowser, (), (const, override));
  MOCK_METHOD(views::View*, TopContainer, (), (override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterActiveTabDidChange,
              (ActiveTabChangeCallback callback),
              (override));
  MOCK_METHOD(tabs::TabInterface*, GetActiveTabInterface, (), (override));
  MOCK_METHOD(BrowserWindowFeatures&, GetFeatures, (), (override));
  MOCK_METHOD(web_modal::WebContentsModalDialogHost*,
              GetWebContentsModalDialogHostForWindow,
              (),
              (override));
  MOCK_METHOD(bool, IsActive, (), (override));
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
  MOCK_METHOD(BrowserUserEducationInterface*,
              GetUserEducationInterface,
              (),
              (override));
  MOCK_METHOD(web_app::AppBrowserController*,
              GetAppBrowserController,
              (),
              (override));

  // PageNavigator methods
  MOCK_METHOD(content::WebContents*,
              OpenURL,
              (const content::OpenURLParams& params,
               base::OnceCallback<void(content::NavigationHandle&)>
                   navigation_handle_callback),
              (override));
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_TEST_MOCK_BROWSER_WINDOW_INTERFACE_H_
