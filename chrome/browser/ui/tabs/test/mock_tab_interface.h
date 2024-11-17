// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TEST_MOCK_TAB_INTERFACE_H_
#define CHROME_BROWSER_UI_TABS_TEST_MOCK_TAB_INTERFACE_H_

#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace tabs {

class MockTabInterface : public testing::NiceMock<TabInterface> {
 public:
  MockTabInterface();
  ~MockTabInterface();

  MOCK_METHOD(content::WebContents*, GetContents, (), (const, override));
  MOCK_METHOD(void, Close, (), (override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterWillDiscardContents,
              (WillDiscardContentsCallback),
              (override));
  MOCK_METHOD(bool, IsInForeground, (), (const, override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterDidEnterForeground,
              (DidEnterForegroundCallback),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterWillEnterBackground,
              (WillEnterBackgroundCallback),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterWillDetach,
              (WillDetach),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterDidInsert,
              (DidInsertCallback),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterPinnedStateChanged,
              (PinnedStateChangedCallback),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterGroupChanged,
              (GroupChangedCallback),
              (override));
  MOCK_METHOD(bool, CanShowModalUI, (), (const, override));
  MOCK_METHOD(std::unique_ptr<ScopedTabModalUI>, ShowModalUI, (), (override));
  MOCK_METHOD(bool, IsInNormalWindow, (), (const override));
  MOCK_METHOD(BrowserWindowInterface*,
              GetBrowserWindowInterface,
              (),
              (override));
  MOCK_METHOD(TabFeatures*, GetTabFeatures, (), (override));
  MOCK_METHOD(std::unique_ptr<views::Widget>,
              CreateAndShowTabScopedWidget,
              (views::WidgetDelegate*),
              (override));
  MOCK_METHOD(bool, IsPinned, (), (const override));
  MOCK_METHOD(std::optional<tab_groups::TabGroupId>,
              GetGroup,
              (),
              (const override));
  MOCK_METHOD(uint32_t, GetTabHandle, (), (const override));
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TEST_MOCK_TAB_INTERFACE_H_
