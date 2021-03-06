// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_BUBBLE_VIEW_H_

#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view_model.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_item_view.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/flags_state.h"
#include "components/flags_ui/flags_storage.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

class Browser;

// TODO(elainechien): Use composition instead of inheritance.
class ChromeLabsBubbleView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(ChromeLabsBubbleView);
  static void Show(views::View* anchor_view,
                   Browser* browser,
                   const ChromeLabsBubbleViewModel* model);

  static bool IsShowing();

  static void Hide();

  ~ChromeLabsBubbleView() override;

  // Getter functions for testing.
  static ChromeLabsBubbleView* GetChromeLabsBubbleViewForTesting();
  flags_ui::FlagsState* GetFlagsStateForTesting();
  flags_ui::FlagsStorage* GetFlagsStorageForTesting();
  views::View* GetMenuItemContainerForTesting();
  bool IsRestartPromptVisibleForTesting();

 private:
  ChromeLabsBubbleView(views::View* anchor_view,
                       Browser* browser,
                       const ChromeLabsBubbleViewModel* model);

  std::unique_ptr<ChromeLabsItemView> CreateLabItem(
      const LabInfo& lab,
      int default_index,
      const flags_ui::FeatureEntry* entry,
      Browser* browser);

  int GetIndexOfEnabledLabState(const flags_ui::FeatureEntry* entry);

  bool IsFeatureSupportedOnChannel(const LabInfo& lab);

  bool IsFeatureSupportedOnPlatform(const flags_ui::FeatureEntry* entry);

  void ShowRelaunchPrompt();

  std::unique_ptr<flags_ui::FlagsStorage> flags_storage_;

  flags_ui::FlagsState* flags_state_;

  // This view will hold all the child lab items.
  views::FlexLayoutView* menu_item_container_;

  const ChromeLabsBubbleViewModel* model_;

  views::View* restart_prompt_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_BUBBLE_VIEW_H_
