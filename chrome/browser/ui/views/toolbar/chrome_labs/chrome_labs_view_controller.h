// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_CHROME_LABS_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_CHROME_LABS_VIEW_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "components/user_education/common/new_badge_controller.h"

class Browser;
class ChromeLabsModel;
class ChromeLabsBubbleView;
struct LabInfo;
class Profile;

namespace flags_ui {
class FlagsState;
class FlagsStorage;
struct FeatureEntry;
}  // namespace flags_ui

class ChromeLabsViewController {
 public:
  ChromeLabsViewController(const ChromeLabsModel* model,
                           ChromeLabsBubbleView* chrome_labs_bubble_view,
                           Browser* browser,
                           flags_ui::FlagsState* flags_state,
                           flags_ui::FlagsStorage* flags_storage);
  ~ChromeLabsViewController() = default;

  void RestartToApplyFlagsForTesting();

 private:
  int GetIndexOfEnabledLabState(const flags_ui::FeatureEntry* entry,
                                flags_ui::FlagsState* flags_state,
                                flags_ui::FlagsStorage* flags_storage);

  void ParseModelDataAndAddLabs();

  void RestartToApplyFlags();

  void SetRestartCallback();

  user_education::DisplayNewBadge ShouldLabShowNewBadge(Profile* profile,
                                                        const LabInfo& lab);

  raw_ptr<const ChromeLabsModel, DanglingUntriaged> model_;
  raw_ptr<ChromeLabsBubbleView, AcrossTasksDanglingUntriaged>
      chrome_labs_bubble_view_;
  base::CallbackListSubscription restart_callback_;
  raw_ptr<Browser, DanglingUntriaged> browser_;
  raw_ptr<flags_ui::FlagsState, DanglingUntriaged> flags_state_;
  raw_ptr<flags_ui::FlagsStorage, DanglingUntriaged> flags_storage_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_CHROME_LABS_VIEW_CONTROLLER_H_
