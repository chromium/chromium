// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_ACTION_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_ACTION_CONTAINER_H_

#include "base/gtest_prod_util.h"
#include "chrome/browser/glic/browser_ui/glic_button_controller_delegate.h"
#include "chrome/browser/ui/tabs/glic_nudge_controller.h"
#include "chrome/browser/ui/tabs/glic_nudge_observer.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_observer.h"
#include "chrome/browser/ui/views/tabs/tab_search_container.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/common/buildflags.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/controls/separator.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/view.h"

namespace gfx {
class Insets;
}
namespace glic {
class GlicButton;
}
class ProductSpecificationsButton;

class TabStripActionContainer : public views::View,
                                public TabDeclutterObserver,
                                public views::AnimationDelegateViews,
                                public views::MouseWatcherListener,
                                public TabOrganizationObserver,
                                public GlicNudgeObserver,
                                public glic::GlicButtonControllerDelegate {
  METADATA_HEADER(TabStripActionContainer, views::View)

 public:
  class TabStripNudgeAnimationSession {
   public:
    enum class AnimationSessionType { SHOW, HIDE };

    TabStripNudgeAnimationSession(TabStripNudgeButton* button,
                                  TabStripActionContainer* container,
                                  AnimationSessionType session_type,
                                  base::OnceCallback<void()> on_animation_ended,
                                  bool animate_opacity = true);
    ~TabStripNudgeAnimationSession();
    void ApplyAnimationValue(const gfx::Animation* animation);
    void MarkAnimationDone(const gfx::Animation* animation);
    void Start();
    AnimationSessionType session_type() { return session_type_; }

    gfx::SlideAnimation* expansion_animation() { return &expansion_animation_; }
    void ResetExpansionAnimationForTesting(double value);
    void ResetOpacityAnimationForTesting(double value);

    void Hide();
    TabStripNudgeButton* button() { return button_; }

   private:
    base::TimeDelta GetAnimationDuration(base::TimeDelta duration);
    void ShowOpacityAnimation();
    void Show();
    raw_ptr<TabStripNudgeButton> button_ = nullptr;
    raw_ptr<TabStripActionContainer> container_ = nullptr;

    gfx::SlideAnimation expansion_animation_;
    gfx::SlideAnimation opacity_animation_;

    bool expansion_animation_done_ = false;
    bool opacity_animation_done_ = false;
    AnimationSessionType session_type_;

    // Timer for initiating the opacity animation during show.
    base::OneShotTimer opacity_animation_delay_timer_;

    // Callback to container after animation has ended.
    base::OnceCallback<void()> on_animation_ended_;

    // Adding boolean since the glic nudge is always opaque.
    bool is_opacity_animated_;

    // track animations to delay posting calls that might delete this class.
    bool is_executing_show_or_hide_ = false;
  };
  explicit TabStripActionContainer(
      TabStripController* tab_strip_controller,
      tabs::TabDeclutterController* tab_declutter_controller,
      tabs::GlicNudgeController* tab_glic_nudge_controller);
  TabStripActionContainer(const TabStripActionContainer&) = delete;
  TabStripActionContainer& operator=(const TabStripActionContainer&) = delete;
  ~TabStripActionContainer() override;

  TabStripNudgeButton* tab_declutter_button() { return tab_declutter_button_; }
  TabStripNudgeButton* auto_tab_group_button() {
    return auto_tab_group_button_;
  }

  TabStripNudgeAnimationSession* animation_session_for_testing() {
    return animation_session_.get();
  }

  TabOrganizationService* tab_organization_service_for_testing() {
    return tab_organization_service_;
  }

  glic::GlicButton* GetGlicButton() { return glic_button_; }

  ProductSpecificationsButton* GetProductSpecificationsButton() {
    return product_specifications_button_;
  }
  // TabOrganizationObserver
  void OnToggleActionUIState(const Browser* browser, bool should_show) override;

  // TabDeclutterObserver
  void OnTriggerDeclutterUIVisibility() override;

  // views::MouseWatcherListener:
  void MouseMovedOutOfHost() override;

  // GlicNudgeObserver
  void OnTriggerGlicNudgeUI(std::string label) override;

  // GlicButtonControllerDelegate:
  void SetGlicShowState(bool show) override;
  void SetGlicIcon(const gfx::VectorIcon& icon) override;

  void UpdateButtonBorders(gfx::Insets button_insets);

  void DidBecomeActive(BrowserWindowInterface* browser);
  void DidBecomeInactive(BrowserWindowInterface* browser);

 private:
  friend class TabStripActionContainerBrowserTest;

  void ShowTabStripNudge(TabStripNudgeButton* button);
  void HideTabStripNudge(TabStripNudgeButton* button);

  // Update the expansion mode to be executed once the mouse is no longer over
  // the nudge.
  void SetLockedExpansionMode(LockedExpansionMode mode,
                              TabStripNudgeButton* button);

#if BUILDFLAG(ENABLE_GLIC)
  std::unique_ptr<glic::GlicButton> CreateGlicButton(
      TabStripController* tab_strip_controller);
  void OnGlicButtonClicked();
  void OnGlicButtonDismissed();
  void OnGlicButtonHovered();
  void OnGlicButtonMouseDown();
#endif

  void OnTabDeclutterButtonClicked();
  void OnTabDeclutterButtonDismissed();

  void OnAutoTabGroupButtonClicked();
  void OnAutoTabGroupButtonDismissed();

  void OnTabStripNudgeButtonTimeout(TabStripNudgeButton* button);

  DeclutterTriggerCTRBucket GetDeclutterTriggerBucket(bool clicked);
  void LogDeclutterTriggerBucket(bool clicked);

  // View where, if the mouse is currently over its bounds, the expansion state
  // will not change. Changes will be staged until after the mouse exits the
  // bounds of this View.
  raw_ptr<View, DanglingUntriaged> locked_expansion_view_ = nullptr;

  // MouseWatcher is used to lock and unlock the expansion state of this
  // container.
  std::unique_ptr<views::MouseWatcher> mouse_watcher_;

  // views::AnimationDelegateViews
  void AnimationCanceled(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  void ExecuteShowTabStripNudge(TabStripNudgeButton* button);
  void ExecuteHideTabStripNudge(TabStripNudgeButton* button);

  void OnAnimationSessionEnded();

  std::unique_ptr<TabStripNudgeButton> CreateAutoTabGroupButton(
      TabStripController* tab_strip_controller);
  std::unique_ptr<TabStripNudgeButton> CreateTabDeclutterButton(
      TabStripController* tab_strip_controller);
  void SetupButtonProperties(TabStripNudgeButton* button);

  // TODO(crbug.com/387356481) make ProductSpecificationsButton a subclass of
  // TabStripNudgeButton
  raw_ptr<ProductSpecificationsButton> product_specifications_button_ = nullptr;
  // The button currently holding the lock to be shown/hidden.
  raw_ptr<TabStripNudgeButton> locked_expansion_button_ = nullptr;
  raw_ptr<TabStripNudgeButton> tab_declutter_button_ = nullptr;
  raw_ptr<TabStripNudgeButton> auto_tab_group_button_ = nullptr;
  raw_ptr<TabOrganizationService> tab_organization_service_ = nullptr;
  raw_ptr<tabs::TabDeclutterController> tab_declutter_controller_ = nullptr;
  raw_ptr<tabs::GlicNudgeController> glic_nudge_controller_ = nullptr;
  raw_ptr<views::Separator> separator_ = nullptr;

  raw_ptr<glic::GlicButton> glic_button_ = nullptr;

  raw_ptr<const Browser> browser_;

  const raw_ptr<TabStripController> tab_strip_controller_ = nullptr;

  // Timer for hiding tab_strip_nudge_button_ after show.
  base::OneShotTimer hide_tab_strip_nudge_timer_;

  // When locked, the container is unable to change its expanded state. Changes
  // will be staged until after this is unlocked.
  LockedExpansionMode locked_expansion_mode_ = LockedExpansionMode::kNone;

  base::ScopedObservation<TabOrganizationService, TabOrganizationObserver>
      tab_organization_observation_{this};

  base::ScopedObservation<tabs::TabDeclutterController, TabDeclutterObserver>
      tab_declutter_observation_{this};

  base::ScopedObservation<tabs::GlicNudgeController, GlicNudgeObserver>
      tab_glic_nudge_observation_{this};

  // Prevents other features from showing tabstrip-modal UI.
  std::unique_ptr<ScopedTabStripModalUI> scoped_tab_strip_modal_ui_;

  std::list<base::CallbackListSubscription> subscriptions_;

  std::unique_ptr<TabStripNudgeAnimationSession> animation_session_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_ACTION_CONTAINER_H_
