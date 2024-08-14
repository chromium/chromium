// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_CHROME_LABS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_CHROME_LABS_BUBBLE_VIEW_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "components/flags_ui/feature_entry.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Browser;
class ChromeLabsItemView;
struct LabInfo;

namespace views {
class Button;
class FlexLayoutView;
}

// TODO(elainechien): Use composition instead of inheritance.
class ChromeLabsBubbleView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(ChromeLabsBubbleView, views::BubbleDialogDelegateView)

 public:
  explicit ChromeLabsBubbleView(views::Button* anchor_view, Browser* browser);
  ~ChromeLabsBubbleView() override;

  ChromeLabsItemView* AddLabItem(
      const LabInfo& lab,
      int default_index,
      const flags_ui::FeatureEntry* entry,
      Browser* browser,
      base::RepeatingCallback<void(ChromeLabsItemView* item_view)>
          combobox_callback);

  size_t GetNumLabItems();

  base::CallbackListSubscription RegisterRestartCallback(
      base::RepeatingClosureList::CallbackType callback);

  void ShowRelaunchPrompt();

  // Getter functions for testing.
  static ChromeLabsBubbleView* GetChromeLabsBubbleViewForTesting();
  views::View* GetMenuItemContainerForTesting();
  bool IsRestartPromptVisibleForTesting();
  views::ScrollView* GetScrollViewForTesting();

 private:
  void NotifyRestartCallback();

  raw_ptr<actions::ActionItem> chrome_labs_action_item_ = nullptr;

  raw_ptr<Browser> browser_;

  // This view will hold all the child lab items.
  raw_ptr<views::FlexLayoutView> menu_item_container_;

  raw_ptr<views::View> restart_prompt_;

  raw_ptr<views::ScrollView> scroll_view_;

  base::RepeatingClosureList restart_callback_list_;
};
#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_CHROME_LABS_BUBBLE_VIEW_H_
