// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_CONTAINER_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_observer.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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

namespace tabs {
class TabDeclutterController;
}

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
        TabOrganizationButton* button,
        TabSearchContainer* container,
        AnimationSessionType session_type,
        base::OnceCallback<void()> on_animation_ended);
    ~TabOrganizationAnimationSession();
    void ApplyAnimationValue(const gfx::Animation* animation);
    void MarkAnimationDone(const gfx::Animation* animation);
    void Start();
    AnimationSessionType session_type() { return session_type_; }

    gfx::SlideAnimation* expansion_animation() { return &expansion_animation_; }
    void ResetAnimationForTesting(double value);

    void Hide();
    TabOrganizationButton* button() { return button_; }

   private:
    base::TimeDelta GetAnimationDuration(base::TimeDelta duration);
    void ShowOpacityAnimation();
    void Show();
    raw_ptr<TabOrganizationButton> button_;
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

  TabSearchContainer(TabStripController* tab_strip_controller,
                     TabStripModel* tab_strip_model,
                     bool before_tab_strip,
                     View* locked_expansion_view,
                     tabs::TabDeclutterController* tab_declutter_controller);
  TabSearchContainer(const TabSearchContainer&) = delete;
  TabSearchContainer& operator=(const TabSearchContainer&) = delete;
  ~TabSearchContainer() override;

  TabOrganizationButton* auto_tab_group_button() {
    return auto_tab_group_button_;
  }

  TabOrganizationButton* tab_declutter_button() {
    return tab_declutter_button_;
  }

  TabSearchButton* tab_search_button() { return tab_search_button_; }

  TabOrganizationAnimationSession* animation_session_for_testing() {
    return animation_session_.get();
  }

  TabOrganizationService* tab_organization_service_for_testing() {
    return tab_organization_service_;
  }

  void ShowTabOrganization(TabOrganizationButton* button);
  void HideTabOrganization(TabOrganizationButton* button);
  void SetLockedExpansionModeForTesting(LockedExpansionMode mode,
                                        TabOrganizationButton* button);

  void OnAutoTabGroupButtonClicked();
  void OnAutoTabGroupButtonDismissed();

  void OnTabDeclutterButtonClicked();
  void OnTabDeclutterButtonDismissed();

  void OnOrganizeButtonTimeout(TabOrganizationButton* button);

  // views::MouseWatcherListener:
  void MouseMovedOutOfHost() override;

  // views::AnimationDelegateViews
  void AnimationCanceled(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  // TabOrganizationObserver
  void OnToggleActionUIState(const Browser* browser, bool should_show) override;

  // TabDeclutterObserver
  void OnTriggerDeclutterUIVisibility(bool should_show) override;

 private:
  void SetLockedExpansionMode(LockedExpansionMode mode,
                              TabOrganizationButton* button);
  void ExecuteShowTabOrganization(TabOrganizationButton* button);
  void ExecuteHideTabOrganization(TabOrganizationButton* button);

  void OnAnimationSessionEnded();

  std::unique_ptr<TabOrganizationButton> CreateAutoTabGroupButton(
      TabStripController* tab_strip_controller,
      bool before_tab_strip);
  std::unique_ptr<TabOrganizationButton> CreateTabDeclutterButton(
      TabStripController* tab_strip_controller,
      bool before_tab_strip);
  void SetupButtonProperties(TabOrganizationButton* button,
                             bool before_tab_strip);

  // View where, if the mouse is currently over its bounds, the expansion state
  // will not change. Changes will be staged until after the mouse exits the
  // bounds of this View.
  raw_ptr<View, DanglingUntriaged> locked_expansion_view_;

  // The button currently holding the lock to be shown/hidden.
  raw_ptr<TabOrganizationButton> locked_expansion_button_ = nullptr;
  raw_ptr<TabOrganizationButton, DanglingUntriaged> auto_tab_group_button_ =
      nullptr;
  raw_ptr<TabOrganizationButton> tab_declutter_button_ = nullptr;
  raw_ptr<TabSearchButton, DanglingUntriaged> tab_search_button_ = nullptr;
  raw_ptr<TabOrganizationService, DanglingUntriaged> tab_organization_service_ =
      nullptr;
  raw_ptr<tabs::TabDeclutterController> tab_declutter_controller_;

  raw_ptr<const Browser> browser_;
  const raw_ptr<TabStripModel> tab_strip_model_;

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
