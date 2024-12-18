// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_glic_container.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#endif  // BUILDFLAG(ENABLE_GLIC)
namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TriggerOutcome {
  kAccepted = 0,
  kDismissed = 1,
  kTimedOut = 2,
  kMaxValue = kTimedOut,
};

constexpr base::TimeDelta kExpansionInDuration = base::Milliseconds(500);
constexpr base::TimeDelta kExpansionOutDuration = base::Milliseconds(250);
constexpr base::TimeDelta kOpacityInDuration = base::Milliseconds(300);
constexpr base::TimeDelta kOpacityOutDuration = base::Milliseconds(100);
constexpr base::TimeDelta kOpacityDelay = base::Milliseconds(100);
constexpr base::TimeDelta kShowDuration = base::Seconds(16);
constexpr char kDeclutterTriggerOutcomeName[] =
    "Tab.Organization.Declutter.Trigger.Outcome";
constexpr char kDeclutterTriggerBucketedCTRName[] =
    "Tab.Organization.Declutter.Trigger.BucketedCTR";
constexpr int kLargeSpaceBetweenButtons = 4;

}  // namespace

TabGlicContainer::TabStripNudgeAnimationSession::TabStripNudgeAnimationSession(
    TabStripNudgeButton* button,
    TabGlicContainer* container,
    AnimationSessionType session_type,
    base::OnceCallback<void()> on_animation_ended)
    : button_(button),
      container_(container),
      expansion_animation_(container),
      opacity_animation_(container),
      session_type_(session_type),
      on_animation_ended_(std::move(on_animation_ended)) {
  if (session_type_ == AnimationSessionType::HIDE) {
    expansion_animation_.Reset(1);
    opacity_animation_.Reset(1);
  }
}

TabGlicContainer::TabStripNudgeAnimationSession::
    ~TabStripNudgeAnimationSession() = default;

void TabGlicContainer::TabStripNudgeAnimationSession::Start() {
  if (session_type_ ==
      TabStripNudgeAnimationSession::AnimationSessionType::SHOW) {
    Show();
  } else {
    Hide();
  }
}

void TabGlicContainer::TabStripNudgeAnimationSession::ResetAnimationForTesting(
    double value) {
  if (opacity_animation_delay_timer_.IsRunning()) {
    opacity_animation_delay_timer_.FireNow();
  }

  expansion_animation_.Reset(value);
  opacity_animation_.Reset(value);
}

void TabGlicContainer::TabStripNudgeAnimationSession::Show() {
  expansion_animation_.SetTweenType(gfx::Tween::Type::ACCEL_20_DECEL_100);
  opacity_animation_.SetTweenType(gfx::Tween::Type::LINEAR);

  expansion_animation_.SetSlideDuration(
      GetAnimationDuration(kExpansionInDuration));
  opacity_animation_.SetSlideDuration(GetAnimationDuration(kOpacityInDuration));

  expansion_animation_.Show();

  const base::TimeDelta delay = GetAnimationDuration(kOpacityDelay);
  opacity_animation_delay_timer_.Start(
      FROM_HERE, delay, this,
      &TabGlicContainer::TabStripNudgeAnimationSession::ShowOpacityAnimation);
}

void TabGlicContainer::TabStripNudgeAnimationSession::Hide() {
  // Animate and hide existing chip.
  if (session_type_ ==
      TabStripNudgeAnimationSession::AnimationSessionType::SHOW) {
    if (opacity_animation_delay_timer_.IsRunning()) {
      opacity_animation_delay_timer_.FireNow();
    }
    session_type_ = TabStripNudgeAnimationSession::AnimationSessionType::HIDE;
  }

  expansion_animation_.SetTweenType(gfx::Tween::Type::ACCEL_20_DECEL_100);
  opacity_animation_.SetTweenType(gfx::Tween::Type::LINEAR);

  expansion_animation_.SetSlideDuration(
      GetAnimationDuration(kExpansionOutDuration));

  opacity_animation_.SetSlideDuration(
      GetAnimationDuration(kOpacityOutDuration));

  expansion_animation_.Hide();
  opacity_animation_.Hide();
}

base::TimeDelta
TabGlicContainer::TabStripNudgeAnimationSession::GetAnimationDuration(
    base::TimeDelta duration) {
  return gfx::Animation::ShouldRenderRichAnimation() ? duration
                                                     : base::TimeDelta();
}

void TabGlicContainer::TabStripNudgeAnimationSession::ShowOpacityAnimation() {
  opacity_animation_.Show();
}

void TabGlicContainer::TabStripNudgeAnimationSession::ApplyAnimationValue(
    const gfx::Animation* animation) {
  float value = animation->GetCurrentValue();
  if (animation == &expansion_animation_) {
    button_->SetWidthFactor(value);
  } else if (animation == &opacity_animation_) {
    button_->SetOpacity(value);
  }
}

void TabGlicContainer::TabStripNudgeAnimationSession::MarkAnimationDone(
    const gfx::Animation* animation) {
  if (animation == &expansion_animation_) {
    expansion_animation_done_ = true;
  } else {
    opacity_animation_done_ = true;
  }

  if (expansion_animation_done_ && opacity_animation_done_) {
    if (on_animation_ended_) {
      std::move(on_animation_ended_).Run();
    }
  }
}

TabGlicContainer::TabGlicContainer(
    TabStripController* tab_strip_controller,
    tabs::TabDeclutterController* tab_declutter_controller)
    : AnimationDelegateViews(this),
      tab_declutter_controller_(tab_declutter_controller) {
  // `tab_declutter_controller_` will be null for some profile types and if
  // feature is not enabled.
  if (tab_declutter_controller_) {
    tab_declutter_button_ =
        AddChildView(CreateTabDeclutterButton(tab_strip_controller));

    SetupButtonProperties(tab_declutter_button_);

    tab_declutter_observation_.Observe(tab_declutter_controller_);
  }

#if BUILDFLAG(ENABLE_GLIC)
  if (GlicEnabling::IsEnabledByFlags()) {
    std::unique_ptr<glic::GlicButton> glic_button =
        std::make_unique<glic::GlicButton>(tab_strip_controller);
    glic_button->SetProperty(views::kCrossAxisAlignmentKey,
                             views::LayoutAlignment::kCenter);
    glic_button->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, 0, 0, GetLayoutConstant(TAB_STRIP_PADDING)));

    glic_button_ = AddChildView(std::move(glic_button));
  }
#endif  // BUILDFLAG(ENABLE_GLIC)

  SetLayoutManager(std::make_unique<views::FlexLayout>());
}

TabGlicContainer::~TabGlicContainer() = default;

std::unique_ptr<TabStripNudgeButton> TabGlicContainer::CreateTabDeclutterButton(
    TabStripController* tab_strip_controller) {
  auto button = std::make_unique<TabStripNudgeButton>(
      tab_strip_controller,
      base::BindRepeating(&TabGlicContainer::OnTabDeclutterButtonClicked,
                          base::Unretained(this)),
      base::BindRepeating(&TabGlicContainer::OnTabDeclutterButtonDismissed,
                          base::Unretained(this)),
      features::IsTabstripDedupeEnabled()
          ? l10n_util::GetStringUTF16(IDS_TAB_DECLUTTER)
          : l10n_util::GetStringUTF16(IDS_TAB_DECLUTTER_NO_DEDUPE),
      kTabDeclutterButtonElementId, Edge::kNone);

  button->SetTooltipText(
      features::IsTabstripDedupeEnabled()
          ? l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_DECLUTTER)
          : l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_DECLUTTER_NO_DEDUPE));
  button->GetViewAccessibility().SetName(
      features::IsTabstripDedupeEnabled()
          ? l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_DECLUTTER)
          : l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_DECLUTTER_NO_DEDUPE));

  button->SetProperty(views::kCrossAxisAlignmentKey,
                      views::LayoutAlignment::kCenter);
  return button;
}

void TabGlicContainer::OnTabDeclutterButtonClicked() {
  tabs::TabDeclutterController::EmitEntryPointHistogram(
      tab_search::mojom::TabDeclutterEntryPoint::kNudge);
  base::UmaHistogramEnumeration(kDeclutterTriggerOutcomeName,
                                TriggerOutcome::kAccepted);
  LogDeclutterTriggerBucket(true);

  ExecuteHideTabStripNudge(tab_declutter_button_);
}

void TabGlicContainer::OnTabDeclutterButtonDismissed() {
  base::UmaHistogramEnumeration(kDeclutterTriggerOutcomeName,
                                TriggerOutcome::kDismissed);
  tab_declutter_controller_->OnActionUIDismissed(
      base::PassKey<TabGlicContainer>());

  ExecuteHideTabStripNudge(tab_declutter_button_);
}

void TabGlicContainer::OnTriggerDeclutterUIVisibility() {
  CHECK(tab_declutter_controller_);

  ShowTabStripNudge(tab_declutter_button_);
}

DeclutterTriggerCTRBucket TabGlicContainer::GetDeclutterTriggerBucket(
    bool clicked) {
  const auto total_tab_count =
      tab_declutter_controller_->tab_strip_model()->GetTabCount();
  const auto stale_tab_count = tab_declutter_controller_->GetStaleTabs().size();

  if (total_tab_count < 15) {
    return clicked ? DeclutterTriggerCTRBucket::kClickedUnder15Tabs
                   : DeclutterTriggerCTRBucket::kShownUnder15Tabs;
  } else if (total_tab_count < 20) {
    if (stale_tab_count < 2) {
      return clicked ? DeclutterTriggerCTRBucket::kClicked15To19TabsUnder2Stale
                     : DeclutterTriggerCTRBucket::kShown15To19TabsUnder2Stale;
    } else if (stale_tab_count < 5) {
      return clicked ? DeclutterTriggerCTRBucket::kClicked15To19Tabs2To4Stale
                     : DeclutterTriggerCTRBucket::kShown15To19Tabs2To4Stale;
    } else if (stale_tab_count < 8) {
      return clicked ? DeclutterTriggerCTRBucket::kClicked15To19Tabs5To7Stale
                     : DeclutterTriggerCTRBucket::kShown15To19Tabs5To7Stale;
    } else {
      return clicked ? DeclutterTriggerCTRBucket::kClicked15To19TabsOver7Stale
                     : DeclutterTriggerCTRBucket::kShown15To19TabsOver7Stale;
    }
  } else if (total_tab_count < 25) {
    if (stale_tab_count < 2) {
      return clicked ? DeclutterTriggerCTRBucket::kClicked20To24TabsUnder2Stale
                     : DeclutterTriggerCTRBucket::kShown20To24TabsUnder2Stale;
    } else if (stale_tab_count < 5) {
      return clicked ? DeclutterTriggerCTRBucket::kClicked20To24Tabs2To4Stale
                     : DeclutterTriggerCTRBucket::kShown20To24Tabs2To4Stale;
    } else if (stale_tab_count < 8) {
      return clicked ? DeclutterTriggerCTRBucket::kClicked20To24Tabs5To7Stale
                     : DeclutterTriggerCTRBucket::kShown20To24Tabs5To7Stale;
    } else {
      return clicked ? DeclutterTriggerCTRBucket::kClicked20To24TabsOver7Stale
                     : DeclutterTriggerCTRBucket::kShown20To24TabsOver7Stale;
    }
  } else {
    return clicked ? DeclutterTriggerCTRBucket::kClickedOver24Tabs
                   : DeclutterTriggerCTRBucket::kShownOver24Tabs;
  }
}

void TabGlicContainer::LogDeclutterTriggerBucket(bool clicked) {
  const DeclutterTriggerCTRBucket bucket = GetDeclutterTriggerBucket(clicked);
  base::UmaHistogramEnumeration(kDeclutterTriggerBucketedCTRName, bucket);
}

void TabGlicContainer::ShowTabStripNudge(TabStripNudgeButton* button) {
  // TODO(crbug.com/384099721) add mouse support
  ExecuteShowTabStripNudge(button);
}

void TabGlicContainer::HideTabStripNudge(TabStripNudgeButton* button) {
  // TODO(crbug.com/384099721) add mouse support
  ExecuteHideTabStripNudge(button);
}

void TabGlicContainer::ExecuteShowTabStripNudge(TabStripNudgeButton* button) {
  // TODO(crbug.com/384554420) add modal support.
  animation_session_ = std::make_unique<TabStripNudgeAnimationSession>(
      button, this, TabStripNudgeAnimationSession::AnimationSessionType::SHOW,
      base::BindOnce(&TabGlicContainer::OnAnimationSessionEnded,
                     base::Unretained(this)));
  animation_session_->Start();

  hide_tab_strip_nudge_timer_.Start(
      FROM_HERE, kShowDuration,
      base::BindOnce(&TabGlicContainer::OnTabStripNudgeButtonTimeout,
                     base::Unretained(this), button));

  if (button == tab_declutter_button_) {
    LogDeclutterTriggerBucket(false);
  }
}

void TabGlicContainer::ExecuteHideTabStripNudge(TabStripNudgeButton* button) {
  // Hide the current animation if the shown button is the same button. Do not
  // create a new animation session.
  if (animation_session_ &&
      animation_session_->session_type() ==
          TabStripNudgeAnimationSession::AnimationSessionType::SHOW &&
      animation_session_->button() == button) {
    hide_tab_strip_nudge_timer_.Stop();
    animation_session_->Hide();
    return;
  }

  if (!button->GetVisible()) {
    return;
  }

  // Stop the timer since the chip might be getting hidden on user actions like
  // dismissal or click and not timeout.
  hide_tab_strip_nudge_timer_.Stop();
  animation_session_ = std::make_unique<TabStripNudgeAnimationSession>(
      button, this, TabStripNudgeAnimationSession::AnimationSessionType::HIDE,
      base::BindOnce(&TabGlicContainer::OnAnimationSessionEnded,
                     base::Unretained(this)));
  animation_session_->Start();
}

void TabGlicContainer::OnTabStripNudgeButtonTimeout(
    TabStripNudgeButton* button) {
  if (button == tab_declutter_button_) {
    base::UmaHistogramEnumeration(kDeclutterTriggerOutcomeName,
                                  TriggerOutcome::kTimedOut);
  }

  // Hide the button if not pressed. Use locked expansion mode to avoid
  // disrupting the user.
  HideTabStripNudge(button);
}

void TabGlicContainer::SetupButtonProperties(TabStripNudgeButton* button) {
  // Set the margins for the button
  const int space_between_buttons = kLargeSpaceBetweenButtons;
  gfx::Insets margin;
  margin.set_right(space_between_buttons);
  button->SetProperty(views::kMarginsKey, margin);

  // Set opacity for the button
  button->SetOpacity(0);
}

void TabGlicContainer::AnimationCanceled(const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void TabGlicContainer::AnimationEnded(const gfx::Animation* animation) {
  animation_session_->ApplyAnimationValue(animation);
  animation_session_->MarkAnimationDone(animation);
}

void TabGlicContainer::OnAnimationSessionEnded() {
  // TODO(crbug.com/384554420) add modal support.

  animation_session_.reset();
}

void TabGlicContainer::AnimationProgressed(const gfx::Animation* animation) {
  animation_session_->ApplyAnimationValue(animation);
}

BEGIN_METADATA(TabGlicContainer)
END_METADATA
