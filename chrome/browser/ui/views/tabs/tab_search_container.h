// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_CONTAINER_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/view.h"

enum class Edge;
class TabOrganizationButton;
class TabOrganizationService;
class TabSearchButton;
class TabStripController;

enum class LockedExpansionMode {
  kNone = 0,
  kWillShow,
  kWillHide,
};

class TabSearchContainer : public views::View,
                           public views::AnimationDelegateViews,
                           public TabOrganizationObserver,
                           public views::MouseWatcherListener {
 public:
  METADATA_HEADER(TabSearchContainer);
  TabSearchContainer(TabStripController* tab_strip_controller,
                     bool before_tab_strip,
                     View* locked_expansion_view);
  TabSearchContainer(const TabSearchContainer&) = delete;
  TabSearchContainer& operator=(const TabSearchContainer&) = delete;
  ~TabSearchContainer() override;

  TabOrganizationButton* tab_organization_button() {
    return tab_organization_button_;
  }
  TabSearchButton* tab_search_button() { return tab_search_button_; }

  gfx::SlideAnimation* expansion_animation_for_testing() {
    return &expansion_animation_;
  }

  TabOrganizationService* tab_organization_service_for_testing() {
    return tab_organization_service_;
  }

  void ShowTabOrganization();
  void HideTabOrganization();
  void SetLockedExpansionModeForTesting(LockedExpansionMode mode);

  void OnOrganizeButtonClicked();
  void OnOrganizeButtonDismissed();
  void OnOrganizeButtonTimeout();

  // views::MouseWatcherListener:
  void MouseMovedOutOfHost() override;

  // views::AnimationDelegateViews
  void AnimationCanceled(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  // TabOrganizationObserver
  void OnToggleActionUIState(const Browser* browser, bool should_show) override;

 private:
  void SetLockedExpansionMode(LockedExpansionMode mode);
  void ExecuteShowTabOrganization();
  void ExecuteHideTabOrganization();
  void ApplyAnimationValue(float value);

  // View where, if the mouse is currently over its bounds, the expansion state
  // will not change. Changes will be staged until after the mouse exits the
  // bounds of this View.
  raw_ptr<View, DanglingUntriaged> locked_expansion_view_;
  raw_ptr<TabOrganizationButton, DanglingUntriaged> tab_organization_button_ =
      nullptr;
  raw_ptr<TabSearchButton, DanglingUntriaged> tab_search_button_ = nullptr;
  raw_ptr<TabOrganizationService, DanglingUntriaged> tab_organization_service_ =
      nullptr;
  raw_ptr<const Browser> browser_;

  // Animation controlling expansion and collapse of tab_organization_button_.
  gfx::SlideAnimation expansion_animation_{this};

  // Timer for hiding tab_organization_button_ after show.
  base::OneShotTimer hide_tab_organization_timer_;

  // When locked, the container is unable to change its expanded state. Changes
  // will be staged until after this is unlocked.
  LockedExpansionMode locked_expansion_mode_ = LockedExpansionMode::kNone;

  // MouseWatcher is used to lock and unlock the expansion state of this
  // container.
  std::unique_ptr<views::MouseWatcher> mouse_watcher_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_CONTAINER_H_
