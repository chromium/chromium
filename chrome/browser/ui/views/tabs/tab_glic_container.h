// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GLIC_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GLIC_CONTAINER_H_

#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_observer.h"
#include "chrome/browser/ui/views/tabs/tab_search_container.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/common/buildflags.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/view.h"

namespace glic {
class GlicButton;
}

class TabGlicContainer : public views::View,
                         public TabDeclutterObserver,
                         public views::AnimationDelegateViews {
  METADATA_HEADER(TabGlicContainer, views::View)

 public:
  class TabStripNudgeAnimationSession {
   public:
    enum class AnimationSessionType { SHOW, HIDE };

    TabStripNudgeAnimationSession(
        TabStripNudgeButton* button,
        TabGlicContainer* container,
        AnimationSessionType session_type,
        base::OnceCallback<void()> on_animation_ended);
    ~TabStripNudgeAnimationSession();
    void ApplyAnimationValue(const gfx::Animation* animation);
    void MarkAnimationDone(const gfx::Animation* animation);
    void Start();
    AnimationSessionType session_type() { return session_type_; }

    gfx::SlideAnimation* expansion_animation() { return &expansion_animation_; }
    void ResetAnimationForTesting(double value);

    void Hide();
    TabStripNudgeButton* button() { return button_; }

   private:
    base::TimeDelta GetAnimationDuration(base::TimeDelta duration);
    void ShowOpacityAnimation();
    void Show();
    raw_ptr<TabStripNudgeButton> button_;
    raw_ptr<TabGlicContainer> container_;

    gfx::SlideAnimation expansion_animation_;
    gfx::SlideAnimation opacity_animation_;

    bool expansion_animation_done_ = false;
    bool opacity_animation_done_ = false;
    AnimationSessionType session_type_;

    // Timer for initiating the opacity animation during show.
    base::OneShotTimer opacity_animation_delay_timer_;

    // Callback to container after animation has ended.
    base::OnceCallback<void()> on_animation_ended_;
  };
  explicit TabGlicContainer(
      TabStripController* tab_strip_controller,
      tabs::TabDeclutterController* tab_declutter_controller);
  TabGlicContainer(const TabGlicContainer&) = delete;
  TabGlicContainer& operator=(const TabGlicContainer&) = delete;
  ~TabGlicContainer() override;

  TabStripNudgeButton* tab_declutter_button() { return tab_declutter_button_; }

  TabStripNudgeAnimationSession* animation_session_for_testing() {
    return animation_session_.get();
  }
  // TabDeclutterObserver
  void OnTriggerDeclutterUIVisibility() override;
#if BUILDFLAG(ENABLE_GLIC)
  glic::GlicButton* GetGlicButton() { return glic_button_; }
#endif  // BUILDFLAG(ENABLE_GLIC)

 private:
  FRIEND_TEST_ALL_PREFIXES(TabGlicContainerBrowserTest, ShowsDeclutterChip);
  FRIEND_TEST_ALL_PREFIXES(TabGlicContainerBrowserTest,
                           ShowsAndHidesDeclutterChip);
  FRIEND_TEST_ALL_PREFIXES(TabGlicContainerBrowserTest,
                           LogsWhenDeclutterButtonClicked);
  FRIEND_TEST_ALL_PREFIXES(TabGlicContainerBrowserTest,
                           LogsWhenDeclutterButtonDismissed);
  FRIEND_TEST_ALL_PREFIXES(TabGlicContainerBrowserTest,
                           LogsWhenDeclutterButtonTimeout);

  void ShowTabStripNudge(TabStripNudgeButton* button);
  void HideTabStripNudge(TabStripNudgeButton* button);

  void OnTabDeclutterButtonClicked();
  void OnTabDeclutterButtonDismissed();

  void OnTabStripNudgeButtonTimeout(TabStripNudgeButton* button);

  DeclutterTriggerCTRBucket GetDeclutterTriggerBucket(bool clicked);
  void LogDeclutterTriggerBucket(bool clicked);

  // views::AnimationDelegateViews
  void AnimationCanceled(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  void ExecuteShowTabStripNudge(TabStripNudgeButton* button);
  void ExecuteHideTabStripNudge(TabStripNudgeButton* button);

  void OnAnimationSessionEnded();

  std::unique_ptr<TabStripNudgeButton> CreateTabDeclutterButton(
      TabStripController* tab_strip_controller);
  void SetupButtonProperties(TabStripNudgeButton* button);

  raw_ptr<TabStripNudgeButton> tab_declutter_button_ = nullptr;
  raw_ptr<tabs::TabDeclutterController> tab_declutter_controller_;
#if BUILDFLAG(ENABLE_GLIC)
  raw_ptr<glic::GlicButton, DanglingUntriaged> glic_button_ = nullptr;
#endif  // BUILDFLAG(ENABLE_GLIC)

  raw_ptr<const Browser> browser_;

  // Timer for hiding tab_strip_nudge_button_ after show.
  base::OneShotTimer hide_tab_strip_nudge_timer_;

  base::ScopedObservation<tabs::TabDeclutterController, TabDeclutterObserver>
      tab_declutter_observation_{this};

  std::unique_ptr<TabStripNudgeAnimationSession> animation_session_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GLIC_CONTAINER_H_
