// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_MOCK_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_MOCK_SIDE_PANEL_UI_H_

#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class MockSidePanelUI : public SidePanelUI {
 public:
  explicit MockSidePanelUI(ui::UnownedUserDataHost& host);
  ~MockSidePanelUI() override;

  MOCK_METHOD(void,
              Show,
              (SidePanelEntryId entry_id,
               std::optional<SidePanelOpenTrigger> open_trigger,
               bool suppress_animations),
              (override));
  MOCK_METHOD(void,
              Show,
              (SidePanelEntryKey entry_key,
               std::optional<SidePanelOpenTrigger> open_trigger,
               bool suppress_animations),
              (override));
  MOCK_METHOD(void,
              ShowFrom,
              (SidePanelEntryKey entry_key,
               gfx::Rect starting_bounds_in_browser_coordinates),
              (override));
  MOCK_METHOD(void,
              Close,
              (SidePanelEntryHideReason hide_reason, bool suppress_animations),
              (override));
  MOCK_METHOD(void,
              Toggle,
              (SidePanelEntryKey key, SidePanelOpenTrigger open_trigger),
              (override));
  MOCK_METHOD(std::optional<SidePanelEntryId>,
              GetCurrentEntryId,
              (),
              (const, override));
  MOCK_METHOD(int, GetCurrentEntryDefaultContentWidth, (), (const, override));
  MOCK_METHOD(bool, IsSidePanelShowing, (), (const, override));
  MOCK_METHOD(bool,
              IsSidePanelEntryShowing,
              (const SidePanelEntryKey& entry_key),
              (const, override));
  MOCK_METHOD(bool,
              IsSidePanelEntryShowing,
              (const SidePanelEntry::Key& entry_key, bool for_tab),
              (const, override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterSidePanelShown,
              (ShownCallback callback),
              (override));
  MOCK_METHOD(void,
              OnActiveTabChanged,
              (content::WebContents * old_contents,
               content::WebContents* new_contents,
               bool tab_removed_for_deletion),
              (override));
  MOCK_METHOD(content::WebContents*,
              GetWebContentsForTest,
              (SidePanelEntryId id),
              (override));
  MOCK_METHOD(void, DisableAnimationsForTesting, (), (override));
  MOCK_METHOD(void,
              SetNoDelaysForTesting,
              (bool no_delays_for_testing),
              (override));

 private:
  ui::ScopedUnownedUserData<SidePanelUI> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_MOCK_SIDE_PANEL_UI_H_
