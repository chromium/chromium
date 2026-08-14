// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_SPLIT_TABS_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_SPLIT_TABS_BUTTON_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "components/prefs/pref_member.h"
#include "ui/base/interaction/element_identifier.h"

class Browser;
class PinnedToolbarButtonStatusIndicator;
class SplitTabMenuModel;

namespace views {
class MenuModelAdapter;
class MenuRunner;
}  // namespace views

// Pinnable toolbar button that allow users to create a split tab if the current
// tab is not in a split or alter the active split tab state.
class SplitTabsToolbarButton : public ToolbarButton,
                               public TabStripModelObserver {
  METADATA_HEADER(SplitTabsToolbarButton, ToolbarButton)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kUpdatePinStateMenu);

  explicit SplitTabsToolbarButton(Browser* browser);
  SplitTabsToolbarButton(const SplitTabsToolbarButton&) = delete;
  SplitTabsToolbarButton& operator=(const SplitTabsToolbarButton&) = delete;
  ~SplitTabsToolbarButton() override;

  // TabStripModelObserver implementation:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnSplitTabChanged(const SplitTabChange& change) override;

  // ToolbarButton override:
  void Layout(PassKey) override;
  void UpdateIcon() override;

  const std::optional<ToolbarButton::VectorIcons>& GetIconsForTesting();

 private:
  bool IsActiveTabInSplit();
  void ButtonPressed(const ui::Event& event);
  void UpdateButtonVisibility();
  void UpdateButtonIcon();
  void UpdateStatusIndicator(bool show_status_indicator);
  void UpdateAccessibilityRole(bool has_menu);
  void UpdateAccessibilityLabel(bool is_enabled);

  // If the IPH is showing, notifies that the feature promo was used.
  void MaybeNotifyIndirectAccessIPHUsed();

  // Aborts the feature promo if we are no longer in a side-by-side split.
  void MaybeAbortIndirectAccessIPH();

  BooleanPrefMember pin_state_;
  raw_ptr<Browser> browser_;
  raw_ptr<PinnedToolbarButtonStatusIndicator> status_indicator_;
  std::unique_ptr<SplitTabMenuModel> split_tab_menu_;
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_SPLIT_TABS_BUTTON_H_
