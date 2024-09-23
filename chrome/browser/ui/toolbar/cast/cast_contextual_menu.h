// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_CAST_CAST_CONTEXTUAL_MENU_H_
#define CHROME_BROWSER_UI_TOOLBAR_CAST_CAST_CONTEXTUAL_MENU_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;

// The class for the contextual menu for the Cast toolbar icon.
class CastContextualMenu : public ui::SimpleMenuModel::Delegate {
 public:
  class Observer {
   public:
    virtual void OnContextMenuShown() = 0;
    virtual void OnContextMenuHidden() = 0;
  };

  // Creates an instance for a Cast toolbar icon shown in the toolbar.
  // |observer| must outlive the context menu.
  static std::unique_ptr<CastContextualMenu> Create(Browser* browser,
                                                           Observer* observer);

  // Constructor called by the static Create* methods above and tests.
  CastContextualMenu(Browser* browser,
                            bool shown_by_policy,
                            Observer* observer);

  CastContextualMenu(const CastContextualMenu&) = delete;
  CastContextualMenu& operator=(const CastContextualMenu&) =
      delete;

  ~CastContextualMenu() override;

  // Creates a menu model with |this| as its delegate.
  std::unique_ptr<ui::SimpleMenuModel> CreateMenuModel();

 private:
  FRIEND_TEST_ALL_PREFIXES(CastContextualMenuUnitTest,
                           ToggleCloudServicesItem);
  FRIEND_TEST_ALL_PREFIXES(CastContextualMenuUnitTest,
                           ShowCloudServicesDialog);
  FRIEND_TEST_ALL_PREFIXES(CastContextualMenuUnitTest,
                           ToggleAlwaysShowIconItem);
  FRIEND_TEST_ALL_PREFIXES(CastContextualMenuUnitTest,
                           ToggleMediaRemotingItem);
  FRIEND_TEST_ALL_PREFIXES(CastContextualMenuUnitTest,
                           ActionShownByPolicy);
  FRIEND_TEST_ALL_PREFIXES(CastContextualMenuUnitTest,
                           NotifyActionController);

  // Gets or sets the "Always show icon" option.
  bool GetAlwaysShowActionPref() const;
  void SetAlwaysShowActionPref(bool always_show);

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void OnMenuWillShow(ui::SimpleMenuModel* source) override;
  void MenuClosed(ui::SimpleMenuModel* source) override;

  // Toggles the preference to enable or disable media remoting.
  void ToggleMediaRemoting();

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Opens feedback page loaded from the media router extension.
  void ReportIssue();
#endif

  const raw_ptr<Browser> browser_;
  const raw_ptr<Observer, AcrossTasksDanglingUntriaged> observer_;

  // Whether the Cast toolbar icon this context menu is shown for is shown by
  // the administrator policy.
  const bool shown_by_policy_;
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_CAST_CAST_CONTEXTUAL_MENU_H_
