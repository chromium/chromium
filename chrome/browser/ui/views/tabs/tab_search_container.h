// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_CONTAINER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_observer.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/view.h"

enum class Edge;
class BrowserWindowInterface;
class TabStripNudgeButton;
class TabOrganizationService;
class TabSearchButton;
class TabStrip;
class ScopedTabStripModalUI;

namespace tabs {
class TabDeclutterController;
}  // namespace tabs

enum class LockedExpansionMode {
  kNone = 0,
  kWillShow,
  kWillHide,
};

class TabSearchContainer : public views::View,
                           public views::AnimationDelegateViews,
                           public TabOrganizationObserver,
                           public TabDeclutterObserver,
                           public views::MouseWatcherListener {
  METADATA_HEADER(TabSearchContainer, views::View)

 public:
  class TabOrganizationAnimationSession {
   public:
    enum class AnimationSessionType { SHOW, HIDE };

    TabOrganizationAnimationSession(
        TabStripNudgeButton* button,
        TabSearchContainer* container,
        AnimationSessionType session_type,
        base::OnceCallback<void()> on_animation_ended);
    ~TabOrganizationAnimationSession();
    void ApplyAnimationValue(const gfx::Animation* animation);
    void MarkAnimationDone(const gfx::Animation* animation);
    void Start();
    AnimationSessionType session_type() { return session_type_; }

    gfx::SlideAnimation* expansion_animation() { return &expansion_animation_; }
    void ResetExpansionAnimationForTesting(double value);
    void ResetOpacityAnimationForTesting(double value);
    void ResetFlatEdgeAnimationForTesting(double value);

    void Hide();
    TabStripNudgeButton* button() { return button_; }

   private:
    void ShowOpacityAnimation();
    void Show();
    raw_ptr<TabStripNudgeButton> button_;
    raw_ptr<TabSearchContainer> container_;

    gfx::SlideAnimation expansion_animation_;
    gfx::SlideAnimation flat_edge_animation_;
    gfx::SlideAnimation opacity_animation_;

    bool expansion_animation_done_ = false;
    bool flat_edge_animation_done_ = false;
    bool opacity_animation_done_ = false;
    AnimationSessionType session_type_;

    // Timer for initiating the opacity animation during show.
    base::OneShotTimer opacity_animation_delay_timer_;

    // Callback to container after animation has ended.
    base::OnceCallback<void()> on_animation_ended_;
  };

  // TODO(382097906): Pull tabslotcontroller out of tabstrip and pass
  // that instead.
  TabSearchContainer(bool tab_search_before_chips,
                     View* locked_expansion_view,
                     TabStrip* tab_strip);
  TabSearchContainer(const TabSearchContainer&) = delete;
  TabSearchContainer& operator=(const TabSearchContainer&) = delete;
  ~TabSearchContainer() override;

  TabStripNudgeButton* auto_tab_group_button() {
    return auto_tab_group_button_;
  }

  TabStripNudgeButton* tab_declutter_button() { return tab_declutter_button_; }

  TabSearchButton* tab_search_button() { return tab_search_button_; }

  TabOrganizationAnimationSession* animation_session_for_testing() {
    return animation_session_.get();
  }

  TabOrganizationService* tab_organization_service_for_testing() {
    return tab_organization_service_;
  }

  void ShowTabOrganization(TabStripNudgeButton* button);
  void HideTabOrganization(TabStripNudgeButton* button);
  void SetLockedExpansionModeForTesting(LockedExpansionMode mode,
                                        TabStripNudgeButton* button);

  void OnAutoTabGroupButtonClicked();
  void OnAutoTabGroupButtonDismissed();

  void OnTabDeclutterButtonClicked();
  void OnTabDeclutterButtonDismissed();

  void OnOrganizeButtonTimeout(TabStripNudgeButton* button);

  // views::MouseWatcherListener:
  void MouseMovedOutOfHost() override;

  // views::AnimationDelegateViews
  void AnimationCanceled(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  // TabOrganizationObserver
  void OnToggleActionUIState(const Browser* browser, bool should_show) override;

  // TabDeclutterObserver
  void OnTriggerDeclutterUIVisibility() override;

 private:
  void SetLockedExpansionMode(LockedExpansionMode mode,
                              TabStripNudgeButton* button);
  void ExecuteShowTabOrganization(TabStripNudgeButton* button);
  void ExecuteHideTabOrganization(TabStripNudgeButton* button);

  void OnAnimationSessionEnded();

  std::unique_ptr<TabStripNudgeButton> CreateAutoTabGroupButton(
      bool tab_search_before_chips);
  std::unique_ptr<TabStripNudgeButton> CreateTabDeclutterButton(
      bool tab_search_before_chips);
  void SetupButtonProperties(TabStripNudgeButton* button,
                             bool tab_search_before_chips);

  // View where, if the mouse is currently over its bounds, the expansion state
  // will not change. Changes will be staged until after the mouse exits the
  // bounds of this View.
  raw_ptr<View, DanglingUntriaged> locked_expansion_view_;

  // The button currently holding the lock to be shown/hidden.
  raw_ptr<TabStripNudgeButton> locked_expansion_button_ = nullptr;
  raw_ptr<TabStripNudgeButton, DanglingUntriaged> auto_tab_group_button_ =
      nullptr;
  raw_ptr<TabStripNudgeButton> tab_declutter_button_ = nullptr;
  raw_ptr<TabSearchButton, DanglingUntriaged> tab_search_button_ = nullptr;
  raw_ptr<TabOrganizationService, DanglingUntriaged> tab_organization_service_ =
      nullptr;

  raw_ptr<BrowserWindowInterface> browser_window_interface_;

  // Timer for hiding tab_organization_button_ after show.
  base::OneShotTimer hide_tab_organization_timer_;

  // When locked, the container is unable to change its expanded state. Changes
  // will be staged until after this is unlocked.
  LockedExpansionMode locked_expansion_mode_ = LockedExpansionMode::kNone;

  // MouseWatcher is used to lock and unlock the expansion state of this
  // container.
  std::unique_ptr<views::MouseWatcher> mouse_watcher_;

  base::ScopedObservation<TabOrganizationService, TabOrganizationObserver>
      tab_organization_observation_{this};

  base::ScopedObservation<tabs::TabDeclutterController, TabDeclutterObserver>
      tab_declutter_observation_{this};

  // Prevents other features from showing tabstrip-modal UI.
  std::unique_ptr<ScopedTabStripModalUI> scoped_tab_strip_modal_ui_;

  std::unique_ptr<TabOrganizationAnimationSession> animation_session_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_CONTAINER_H_
