// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/pass_key.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/glic_actor_nudge_controller.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager_factory.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/glic/glic_and_actor_buttons_container.h"
#include "chrome/browser/ui/views/tabs/glic/tab_strip_glic_actor_task_icon.h"
#include "chrome/browser/ui/views/tabs/glic/tab_strip_glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/mouse_watcher_view_host.h"
#include "ui/views/view_class_properties.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/feature_list.h"
#include "base/types/expected.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/private_ai/private_ai_service.h"
#include "chrome/browser/private_ai/private_ai_service_factory.h"
#include "components/private_ai/client.h"
#include "components/private_ai/error_code.h"
#include "components/private_ai/features.h"
#endif  // !BUILDFLAG(IS_ANDROID)
namespace {

constexpr base::TimeDelta kExpansionInDuration = base::Milliseconds(500);
constexpr base::TimeDelta kExpansionOutDuration = base::Milliseconds(250);
constexpr base::TimeDelta kOpacityInDuration = base::Milliseconds(300);
constexpr base::TimeDelta kOpacityOutDuration = base::Milliseconds(100);
constexpr base::TimeDelta kOpacityDelay = base::Milliseconds(100);

constexpr int kLargeSpaceBetweenButtons = 6;
constexpr int kInsideBorderAroundGlicButtons = 2;
constexpr int kOutsideBorderAroundGlicButtons = 11;
#if !BUILDFLAG(IS_MAC)
constexpr int kLargeSpaceBetweenSeparatorRight = 8;
constexpr int kLargeSpaceBetweenSeparatorLeft = 2;
#endif  // !BUILDFLAG(IS_MAC)
#if !BUILDFLAG(IS_ANDROID)
void EstablishPrivateAiConnection(Profile* profile) {
  if (!profile) {
    return;
  }
  if (base::FeatureList::IsEnabled(private_ai::kPrivateAi) &&
      base::FeatureList::IsEnabled(
          contextual_cueing::kZeroStateSuggestionsUsePrivateAi)) {
    private_ai::PrivateAiService* private_ai_service =
        private_ai::PrivateAiServiceFactory::GetForProfile(profile);
    if (private_ai_service) {
      private_ai::Client* client = private_ai_service->GetClient();
      if (client) {
        // Eagerly establish the connection.
        client->EstablishConnection();
      }
    }
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

using TaskIconAnimationMode = glic::TabStripGlicActorTaskIcon::AnimationMode;
}  // namespace

TabStripActionContainer::TabStripNudgeAnimationSession::
    TabStripNudgeAnimationSession(TabStripNudgeButton* button,
                                  TabStripActionContainer* container,
                                  AnimationSessionType session_type,
                                  base::OnceCallback<void()> on_animation_ended,
                                  bool is_opacity_animated)
    : button_(button),
      container_(container),
      expansion_animation_(container),
      opacity_animation_(container),
      session_type_(session_type),
      on_animation_ended_(std::move(on_animation_ended)),
      is_opacity_animated_(is_opacity_animated),
      is_executing_show_or_hide_(false) {
  if (session_type_ == AnimationSessionType::kHide) {
    expansion_animation_.Reset(1);
    if (is_opacity_animated) {
      opacity_animation_.Reset(1);
    }
  }
}

TabStripActionContainer::TabStripNudgeAnimationSession::
    ~TabStripNudgeAnimationSession() = default;

void TabStripActionContainer::TabStripNudgeAnimationSession::Start() {
  if (session_type_ ==
      TabStripNudgeAnimationSession::AnimationSessionType::kShow) {
    Show();
  } else {
    Hide();
  }
}

void TabStripActionContainer::TabStripNudgeAnimationSession::
    ResetExpansionAnimationForTesting(double value) {
  expansion_animation_.Reset(value);
}

void TabStripActionContainer::TabStripNudgeAnimationSession::
    ResetOpacityAnimationForTesting(double value) {
  if (is_opacity_animated_) {
    if (opacity_animation_delay_timer_.IsRunning()) {
      opacity_animation_delay_timer_.FireNow();
    }
  }

  if (is_opacity_animated_) {
    opacity_animation_.Reset(value);
  }
}

void TabStripActionContainer::TabStripNudgeAnimationSession::Show() {
  base::AutoReset<bool> resetter(&is_executing_show_or_hide_, true);
  expansion_animation_.SetTweenType(gfx::Tween::Type::ACCEL_20_DECEL_100);
  if (is_opacity_animated_) {
    opacity_animation_.SetTweenType(gfx::Tween::Type::LINEAR);
  }
  expansion_animation_.SetSlideDuration(
      GetAnimationDuration(kExpansionInDuration));
  expansion_animation_.Show();

  if (is_opacity_animated_) {
    opacity_animation_.SetSlideDuration(
        GetAnimationDuration(kOpacityInDuration));

    const base::TimeDelta delay = GetAnimationDuration(kOpacityDelay);
    opacity_animation_delay_timer_.Start(
        FROM_HERE, delay, this,
        &TabStripActionContainer::TabStripNudgeAnimationSession::
            ShowOpacityAnimation);
  }
}

void TabStripActionContainer::TabStripNudgeAnimationSession::Hide() {
  base::AutoReset<bool> resetter(&is_executing_show_or_hide_, true);
  // Animate and hide existing chip.
  if (session_type_ ==
      TabStripNudgeAnimationSession::AnimationSessionType::kShow) {
    if (is_opacity_animated_ && opacity_animation_delay_timer_.IsRunning()) {
      opacity_animation_delay_timer_.FireNow();
    }
    session_type_ = TabStripNudgeAnimationSession::AnimationSessionType::kHide;
  }

  expansion_animation_.SetTweenType(gfx::Tween::Type::ACCEL_20_DECEL_100);

  expansion_animation_.SetSlideDuration(
      GetAnimationDuration(kExpansionOutDuration));
  expansion_animation_.Hide();

  if (is_opacity_animated_) {
    opacity_animation_.SetTweenType(gfx::Tween::Type::LINEAR);
    opacity_animation_.SetSlideDuration(
        GetAnimationDuration(kOpacityOutDuration));
    opacity_animation_.Hide();
  }
}

base::TimeDelta
TabStripActionContainer::TabStripNudgeAnimationSession::GetAnimationDuration(
    base::TimeDelta duration) {
  return gfx::Animation::ShouldRenderRichAnimation() ? duration
                                                     : base::TimeDelta();
}

void TabStripActionContainer::TabStripNudgeAnimationSession::
    ShowOpacityAnimation() {
  opacity_animation_.Show();
}

void TabStripActionContainer::TabStripNudgeAnimationSession::
    ApplyAnimationValue(const gfx::Animation* animation) {
  float value = animation->GetCurrentValue();
  if (animation == &expansion_animation_) {
    button_->SetWidthFactor(value);
  } else if (animation == &opacity_animation_) {
    button_->SetOpacity(value);
  }
}

void TabStripActionContainer::TabStripNudgeAnimationSession::MarkAnimationDone(
    const gfx::Animation* animation) {
  if (animation == &expansion_animation_) {
    expansion_animation_done_ = true;
  } else {
    opacity_animation_done_ = true;
  }

  const bool opacity_animation_not_running =
      opacity_animation_done_ || !is_opacity_animated_;

  if (expansion_animation_done_ && opacity_animation_not_running) {
    if (on_animation_ended_) {
      if (is_executing_show_or_hide_) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, std::move(on_animation_ended_));
      } else {
        std::move(on_animation_ended_).Run();
      }
    }
  }
}

TabStripActionContainer::TabStripActionContainer(
    BrowserWindowInterface* browser_window_interface,
    tabs::GlicNudgeController* glic_nudge_controller)
    : AnimationDelegateViews(this),
      locked_expansion_view_(this),
      glic_nudge_controller_(glic_nudge_controller),
      browser_window_interface_(browser_window_interface) {
  SetProperty(views::kElementIdentifierKey, kTabStripActionContainerElementId);

  mouse_watcher_ = std::make_unique<views::MouseWatcher>(
      std::make_unique<views::MouseWatcherViewHost>(locked_expansion_view_,
                                                    gfx::Insets()),
      this);

  // `glic_nudge_controller_` will be null if feature is not enabled.
  if (glic_nudge_controller_) {
    glic_nudge_controller_->SetDelegate(this);
  }

  if (glic::GlicEnabling::IsProfileEligible(
          browser_window_interface_->GetProfile())) {
    if (base::FeatureList::IsEnabled(features::kGlicActorUi) &&
        features::kGlicActorUiTaskIcon.Get()) {
      glic_actor_button_container_ =
          AddChildView(CreateGlicActorButtonContainer());
      glic_actor_task_icon_ =
          glic_actor_button_container_->AddChildView(CreateGlicActorTaskIcon());
      glic_actor_task_icon_->SetVisible(false);
      glic_actor_button_container_->SetVisible(false);
    }
    glic_button_ = AddChildView(CreateGlicButton());

#if !BUILDFLAG(IS_MAC)
    std::unique_ptr<views::Separator> separator =
        std::make_unique<views::Separator>();
    separator->SetBorderRadius(TabStyle::Get()->GetSeparatorCornerRadius());
    separator->SetPreferredSize(TabStyle::Get()->GetSeparatorSize());

    separator->SetColorId(kColorTabDividerFrameActive);

    gfx::Insets margin;
    margin.set_left_right(kLargeSpaceBetweenSeparatorLeft,
                          kLargeSpaceBetweenSeparatorRight);

    separator->SetProperty(views::kMarginsKey, margin);

    subscriptions_.push_back(browser_window_interface_->RegisterDidBecomeActive(
        base::BindRepeating(&TabStripActionContainer::DidBecomeActive,
                            base::Unretained(this))));
    subscriptions_.push_back(
        browser_window_interface_->RegisterDidBecomeInactive(
            base::BindRepeating(&TabStripActionContainer::DidBecomeInactive,
                                base::Unretained(this))));
    separator_ = AddChildView(std::move(separator));
#endif  // !BUILDFLAG(IS_MAC)
  }
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCollapseMargins(false);
}

TabStripActionContainer::~TabStripActionContainer() {
  if (scoped_tab_strip_modal_ui_) {
    scoped_tab_strip_modal_ui_.reset();
  }
  if (glic_nudge_controller_) {
    glic_nudge_controller_->SetDelegate(nullptr);
  }
}

std::unique_ptr<glic::TabStripGlicButton>
TabStripActionContainer::CreateGlicButton() {
  glic::GlicKeyedService* service =
      glic::GlicKeyedService::Get(browser_window_interface_->GetProfile());
  std::u16string tooltip_text = l10n_util::GetStringUTF16(
      service->IsWindowOrFreShowing() ? IDS_GLIC_TAB_STRIP_BUTTON_TOOLTIP_CLOSE
                                      : IDS_GLIC_TAB_STRIP_BUTTON_TOOLTIP);
  std::unique_ptr<glic::TabStripGlicButton> glic_button =
      std::make_unique<glic::TabStripGlicButton>(
          browser_window_interface_,
          base::BindRepeating(&TabStripActionContainer::OnGlicButtonClicked,
                              base::Unretained(this)),
          base::BindRepeating(&TabStripActionContainer::OnGlicButtonDismissed,
                              base::Unretained(this)),
          base::BindRepeating(&TabStripActionContainer::OnGlicButtonHovered,
                              base::Unretained(this)),
          base::BindRepeating(&TabStripActionContainer::OnGlicButtonMouseDown,
                              base::Unretained(this)),
          base::BindRepeating(
              &TabStripActionContainer::OnGlicButtonAnimationEnded,
              base::Unretained(this)),
          tooltip_text);

  glic_button->SetProperty(views::kCrossAxisAlignmentKey,
                           views::LayoutAlignment::kCenter);

  return glic_button;
}

std::unique_ptr<glic::TabStripGlicActorTaskIcon>
TabStripActionContainer::CreateGlicActorTaskIcon() {
  std::unique_ptr<glic::TabStripGlicActorTaskIcon> glic_actor_task_icon =
      std::make_unique<glic::TabStripGlicActorTaskIcon>(
          browser_window_interface_,
          base::BindRepeating(
              &TabStripActionContainer::OnGlicActorTaskIconClicked,
              base::Unretained(this)));

  glic_actor_task_icon->SetProperty(views::kCrossAxisAlignmentKey,
                                    views::LayoutAlignment::kCenter);

  return glic_actor_task_icon;
}

// TODO(crbug.com/431015299): Clean up when GlicButton and GlicActorTaskIcon
// have been combined.
std::unique_ptr<GlicAndActorButtonsContainer>
TabStripActionContainer::CreateGlicActorButtonContainer() {
  auto glic_actor_button_container =
      std::make_unique<GlicAndActorButtonsContainer>();

  // Should be hidden until a task starts.
  glic_actor_button_container->SetVisible(false);

  return glic_actor_button_container;
}

void TabStripActionContainer::UpdateGlicActorButtonContainerBorders() {
  CHECK(glic_button_);
  gfx::Insets glic_border;

  // Ensure buttons look vertically centered by making the top and bottom insets
  // match.
  gfx::Insets border_insets = border_insets_;
  int min_vertical_inset =
      std::min(border_insets.top(), border_insets.bottom());
  border_insets.set_top_bottom(min_vertical_inset, min_vertical_inset);

  // GlicActorTaskIcon will only ever be shown alongside the GlicButton.
  if (glic_actor_task_icon_ && glic_actor_task_icon_->IsDrawn()) {
    gfx::Insets task_icon_border;
    const gfx::Insets right_icon_border =
        gfx::Insets().set_left_right(0, kOutsideBorderAroundGlicButtons);
    const gfx::Insets left_icon_border = gfx::Insets().set_left_right(
        kOutsideBorderAroundGlicButtons, kInsideBorderAroundGlicButtons);
    task_icon_border = right_icon_border + border_insets;
    glic_border = left_icon_border + border_insets;
    glic_actor_task_icon_->SetBorder(
        views::CreateEmptyBorder(task_icon_border));
    // Force a background repaint to account for the new border insets.
    glic_actor_task_icon_->RefreshBackground();
  } else {
    // Reset GlicButton border if Task Icon is hidden.
    glic_border = gfx::Insets().set_left_right(border_insets.top(),
                                               border_insets.bottom()) +
                  border_insets;
  }
  glic_button_->SetBorder(views::CreateEmptyBorder(glic_border));
  // Force a background repaint to account for the new border insets.
  glic_button_->RefreshBackground();
}

void TabStripActionContainer::OnGlicButtonClicked() {
  // Indicate that the glic button was pressed so that we can either close the
  // IPH promo (if present) or note that it has already been used to prevent
  // unnecessarily displaying the promo.
  BrowserUserEducationInterface::From(browser_window_interface_)
      ->NotifyFeaturePromoFeatureUsed(
          feature_engagement::kIPHGlicPromoFeature,
          FeaturePromoFeatureUsedAction::kClosePromoIfPresent);

  std::optional<std::string> prompt_suggestion;
  if (glic_nudge_controller_) {
    prompt_suggestion = glic_nudge_controller_->GetPromptSuggestion();
    glic_nudge_controller_->ClearPromptSuggestion();
  }
  glic::GlicKeyedServiceFactory::GetGlicKeyedService(
      browser_window_interface_->GetProfile())
      ->ToggleUI(browser_window_interface_,
                 /*prevent_close=*/false,
                 glic_button_->GetIsShowingNudge()
                     ? glic::mojom::InvocationSource::kNudge
                     : glic::mojom::InvocationSource::kTopChromeButton,
                 prompt_suggestion);

  if (glic_button_->GetIsShowingNudge()) {
    glic_nudge_controller_->OnNudgeActivity(
        tabs::GlicNudgeActivity::kNudgeClicked);
  }

  ExecuteHideTabStripNudge(glic_button_);
  // Reset state manually since there wont be a mouse up event as the animation
  // moves the button out of the way.
  glic_button_->SetState(views::Button::ButtonState::STATE_NORMAL);
}

void TabStripActionContainer::OnGlicButtonDismissed() {
  glic_nudge_controller_->OnNudgeActivity(
      tabs::GlicNudgeActivity::kNudgeDismissed);

  // Force hide the button when pressed, bypassing locked expansion mode.
  ExecuteHideTabStripNudge(glic_button_);
}

void TabStripActionContainer::OnGlicButtonHovered() {
  Profile* const profile = browser_window_interface_->GetProfile();
#if !BUILDFLAG(IS_ANDROID)
  EstablishPrivateAiConnection(profile);
#endif  // !BUILDFLAG(IS_ANDROID)
  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  if (auto* instance =
          glic_service->GetInstanceForActiveTab(browser_window_interface_)) {
    instance->host().instance_delegate().PrepareForOpen();
  }
}

void TabStripActionContainer::OnGlicButtonMouseDown() {
  Profile* const profile = browser_window_interface_->GetProfile();
  if (!glic::GlicEnabling::IsEnabledAndConsentForProfile(profile)) {
    // Do not do this optimization if user has not consented to GLIC.
    return;
  }
  auto* glic_service = glic::GlicKeyedService::Get(profile);

  // TODO(crbug.com/445934142): Create the instance here so that suggestions can
  // be fetched, but don't show it yet.
  if (auto* instance =
          glic_service->GetInstanceForActiveTab(browser_window_interface_)) {
    // This prefetches the results and allows the underlying implementation to
    // cache the results for future calls. Which is why the callback does
    // nothing.
    instance->host().instance_delegate().FetchZeroStateSuggestions(
        /*is_first_run=*/false, /*supported_tools=*/std::nullopt,
        base::DoNothing());
  }
}

void TabStripActionContainer::OnGlicButtonAnimationEnded() {
  if (!glic_button_->GetIsShowingNudge()) {
    scoped_tab_strip_modal_ui_.reset();

    if (locked_expansion_button_) {
      locked_expansion_button_->SetIsShowingNudge(false);
    }
  }
}

void TabStripActionContainer::OnGlicActorTaskIconClicked() {
  Profile* const profile = browser_window_interface_->GetProfile();
  auto* icon_manager =
      tabs::GlicActorTaskIconManagerFactory::GetForProfile(profile);
  CHECK(icon_manager);

  ActorTaskListBubbleController* controller =
      ActorTaskListBubbleController::From(browser_window_interface_);
  controller->ShowBubble(glic_actor_task_icon_);

  auto current_task_nudge_state = icon_manager->GetCurrentActorTaskNudgeState();
    actor::ui::LogGlobalTaskIndicatorClick(current_task_nudge_state);
}

void TabStripActionContainer::OnTriggerGlicNudgeUI(std::string label) {
  if (GetIsShowingGlicActorTaskIconNudge()) {
    return;
  }

  CHECK(glic_button_);
  if (!label.empty()) {
    glic_button_->SetNudgeLabel(std::move(label));
    ShowTabStripNudge(glic_button_);
  }
}

void TabStripActionContainer::OnHideGlicNudgeUI() {
  CHECK(glic_button_);
  HideTabStripNudge(glic_button_);
}

bool TabStripActionContainer::GetIsShowingGlicNudge() {
  return glic_button_ && glic_button_->GetIsShowingNudge();
}

views::FlexLayoutView* TabStripActionContainer::glic_actor_button_container() {
  return glic_actor_button_container_;
}

void TabStripActionContainer::TriggerGlicActorNudge(
    const std::u16string nudge_text) {
  CHECK(glic_actor_task_icon_);
  if (GetIsShowingGlicNudge()) {
    // If the glic button is showing, start the hide animation in parallel to
    // the show actor nudge animation.
    HideTabStripNudge(glic_button_);
    OnGlicButtonAnimationEnded();
  }
  ShowGlicActorNudge(nudge_text);
}

void TabStripActionContainer::ShowGlicActorNudge(
    const std::u16string nudge_text) {
  CHECK(glic_actor_task_icon_);
  // Start animation for minimizing the glic button.
  glic_button_->Collapse();
  ShowGlicActorTaskIcon();
  glic_actor_task_icon_->ShowNudgeLabel(nudge_text);
  ShowTabStripNudge(glic_actor_task_icon_);
}

void TabStripActionContainer::ShowGlicActorTaskIcon() {
  CHECK(glic_actor_button_container_);
  CHECK(glic_button_);
  // If the nudge is showing (ex: previous state was CheckTasks), hide the
  // nudge.
  if (glic_actor_task_icon_->GetIsShowingNudge()) {
    HideTabStripNudge(glic_actor_task_icon_);
    return;
  }
  glic_button_ = glic_actor_button_container_->InsertGlicButton(glic_button_);
  glic_actor_task_icon_->SetVisible(true);
  glic_actor_button_container_->SetVisible(true);
  glic_button_->Collapse();
  glic_button_->SetSplitButtonCornerStyling();
  UpdateGlicActorButtonContainerBorders();

  // If in entry mode, attempt to animate the icon's appearance. If the tab
  // strip is blocked (e.g. by another nudge), skip the entrance animation and
  // jump straight to the nudge state to ensure the icon is visible.
  if (glic_actor_task_icon_->GetAnimationMode() ==
      TaskIconAnimationMode::kEntry) {
    if (browser_window_interface_->GetTabStripModel()->CanShowModalUI()) {
      scoped_tab_strip_modal_ui_ =
          browser_window_interface_->GetTabStripModel()->ShowModalUI();
      animation_session_ = std::make_unique<TabStripNudgeAnimationSession>(
          glic_actor_task_icon_, this,
          TabStripNudgeAnimationSession::AnimationSessionType::kShow,
          base::BindOnce(&TabStripActionContainer::OnAnimationSessionEnded,
                         weak_factory_.GetWeakPtr()),
          /*is_opacity_animated=*/false);
      animation_session_->Start();
    } else {
      glic_actor_task_icon_->SetAnimationMode(TaskIconAnimationMode::kNudge);
    }
  }
}

void TabStripActionContainer::HideGlicActorTaskIcon() {
  CHECK(glic_actor_button_container_);
  CHECK(glic_button_);
  CHECK(glic_actor_task_icon_);

    // If it's already hidden, do nothing.
    if (!glic_actor_task_icon_->GetVisible()) {
      return;
    }
    glic_actor_task_icon_->SetIsShowingNudge(false);
    glic_actor_task_icon_->SetAnimationMode(TaskIconAnimationMode::kEntry);
    if (browser_window_interface_->GetTabStripModel()->CanShowModalUI()) {
      scoped_tab_strip_modal_ui_ =
          browser_window_interface_->GetTabStripModel()->ShowModalUI();

      animation_session_ = std::make_unique<TabStripNudgeAnimationSession>(
          glic_actor_task_icon_, this,
          TabStripNudgeAnimationSession::AnimationSessionType::kHide,
          base::BindOnce(&TabStripActionContainer::OnAnimationSessionEnded,
                         weak_factory_.GetWeakPtr()),
          /*is_opacity_animated=*/false);
      animation_session_->Start();
      return;
    }
  // If animation isn't possible, snap hide immediately.
  FinalizeHideGlicActorTaskIcon();
}

void TabStripActionContainer::FinalizeHideGlicActorTaskIcon() {
  // 1. Reset Nudge State
  if (glic_actor_task_icon_->GetIsShowingNudge()) {
    if (animation_session_ &&
        animation_session_->button() == glic_actor_task_icon_) {
      animation_session_.reset();
    }
    glic_actor_task_icon_->SetIsShowingNudge(false);
  }
  glic_actor_task_icon_->SetVisible(false);
  glic_actor_task_icon_->SetTaskIconToDefault();
  glic_button_ = AddChildView(std::move(glic_button_));
  glic_actor_button_container_->SetVisible(false);
  glic_button_->Expand();
  glic_button_->ResetSplitButtonCornerStyling();
  // Reset the animation mode for the next time the icon is shown.
  glic_actor_task_icon_->SetAnimationMode(TaskIconAnimationMode::kEntry);
  UpdateGlicActorButtonContainerBorders();
#if !BUILDFLAG(IS_MAC)
  // Re-add the separator so it's ordered after the GlicButton.
  separator_ = AddChildView(std::move(separator_));
#endif  // !BUILDFLAG(IS_MAC)
}

bool TabStripActionContainer::GetIsShowingGlicActorTaskIconNudge() {
  return glic_actor_task_icon_ && glic_actor_task_icon_->GetIsShowingNudge();
}

void TabStripActionContainer::ShowTabStripNudge(TabStripNudgeButton* button) {
  if (locked_expansion_view_->IsMouseHovered()) {
    SetLockedExpansionMode(LockedExpansionMode::kWillShow, button);
  }
  if (locked_expansion_mode_ == LockedExpansionMode::kNone) {
    ExecuteShowTabStripNudge(button);
  }
}

void TabStripActionContainer::HideTabStripNudge(TabStripNudgeButton* button) {
  if (locked_expansion_view_->IsMouseHovered()) {
    SetLockedExpansionMode(LockedExpansionMode::kWillHide, button);
  }
  if (locked_expansion_mode_ == LockedExpansionMode::kNone) {
    ExecuteHideTabStripNudge(button);
  }
}

void TabStripActionContainer::ExecuteShowTabStripNudge(
    TabStripNudgeButton* button) {
  bool can_show_modal_ui =
      browser_window_interface_->GetTabStripModel()->CanShowModalUI();
  // The tab strip might currently be animating a hide for this button. If we
  // receive a show request during this time, we allow it to interrupt the hide
  // animation so the button can re-expand immediately.
  if (!can_show_modal_ui && animation_session_ &&
      animation_session_->session_type() ==
          TabStripNudgeAnimationSession::AnimationSessionType::kHide &&
      animation_session_->button() == button) {
    can_show_modal_ui = true;
  }
  // The glic actor icon might currently be running its entrance animation. If
  // we receive a nudge request during this time, we allow it to interrupt the
  // entrance animation so the text can appear immediately alongside the icon.
  if (!can_show_modal_ui && animation_session_ &&
      animation_session_->button() == glic_actor_task_icon_ &&
      animation_session_->session_type() ==
          TabStripNudgeAnimationSession::AnimationSessionType::kShow &&
      glic_actor_task_icon_->GetAnimationMode() ==
          TaskIconAnimationMode::kEntry) {
    can_show_modal_ui = true;
  }
  if (!can_show_modal_ui) {
    return;
  }

  button->SetIsShowingNudge(true);

  // Only change the margins between the GlicButton and nudges that are NOT
  // coming from the GlicActorTaskIcon.
  if (glic_button_ && glic_button_->GetVisible() && button != glic_button_ &&
      button != glic_actor_task_icon_) {
    const int space_between_buttons = kLargeSpaceBetweenButtons;
    gfx::Insets margin;
    margin.set_right(space_between_buttons);
    button->SetProperty(views::kMarginsKey, margin);
  } else {
    // Reset the margins.
    button->SetProperty(views::kMarginsKey, gfx::Insets());
  }
  scoped_tab_strip_modal_ui_.reset();
  scoped_tab_strip_modal_ui_ =
      browser_window_interface_->GetTabStripModel()->ShowModalUI();

  if (!ButtonOwnsAnimation(button)) {
    animation_session_ = std::make_unique<TabStripNudgeAnimationSession>(
        button, this,
        TabStripNudgeAnimationSession::AnimationSessionType::kShow,
        base::BindOnce(&TabStripActionContainer::OnAnimationSessionEnded,
                       base::Unretained(this)),
        (button != glic_button_ && button != glic_actor_task_icon_));
    animation_session_->Start();
  }
}

void TabStripActionContainer::ExecuteHideTabStripNudge(
    TabStripNudgeButton* button) {
  // Hide the current animation if the shown button is the same button. Do not
  // create a new animation session.
  if (animation_session_ &&
      animation_session_->session_type() ==
          TabStripNudgeAnimationSession::AnimationSessionType::kShow &&
      animation_session_->button() == button) {
    hide_tab_strip_nudge_timer_.Stop();
    animation_session_->Hide();
    return;
  }

  if (!button->GetVisible()) {
    return;
  }
  // Since the glic button is still visible in it's hidden state we need to have
  // a special case to query if it's in its Hide state.
  if (button == glic_button_ && button->GetWidthFactor() == 0.0 &&
      !ButtonOwnsAnimation(button)) {
    return;
  }
  const bool is_actor_nudge_mode = (button == glic_actor_task_icon_ &&
                                    glic_actor_task_icon_->GetAnimationMode() ==
                                        TaskIconAnimationMode::kNudge);
  // For actor icon in nudge mode, only animate if the nudge text is actually
  // showing.
  if (is_actor_nudge_mode && !button->GetIsShowingNudge()) {
    return;
  }
  // The actor icon nudge must keep the text for the animation duration so it
  // slides out visibly.
  if (!is_actor_nudge_mode) {
    button->SetIsShowingNudge(false);
  }

  // Stop the timer since the chip might be getting hidden on user actions like
  // dismissal or click and not timeout.
  hide_tab_strip_nudge_timer_.Stop();
  if (!ButtonOwnsAnimation(button)) {
    animation_session_ = std::make_unique<TabStripNudgeAnimationSession>(
        button, this,
        TabStripNudgeAnimationSession::AnimationSessionType::kHide,
        base::BindOnce(&TabStripActionContainer::OnAnimationSessionEnded,
                       base::Unretained(this)),
        (button != glic_button_ && button != glic_actor_task_icon_));
    animation_session_->Start();
  }
}

void TabStripActionContainer::SetLockedExpansionMode(
    LockedExpansionMode mode,
    TabStripNudgeButton* button) {
  if (mode == LockedExpansionMode::kNone) {
    if (locked_expansion_mode_ == LockedExpansionMode::kWillShow) {
      ExecuteShowTabStripNudge(locked_expansion_button_);
    } else if (locked_expansion_mode_ == LockedExpansionMode::kWillHide) {
      ExecuteHideTabStripNudge(locked_expansion_button_);
    }
    locked_expansion_button_ = nullptr;
  } else {
    locked_expansion_button_ = button;
    mouse_watcher_->Start(GetWidget()->GetNativeWindow());
  }
  locked_expansion_mode_ = mode;
}

void TabStripActionContainer::OnTabStripNudgeButtonTimeout(
    TabStripNudgeButton* button) {
  // Hide the button if not pressed. Use locked expansion mode to avoid
  // disrupting the user.
  HideTabStripNudge(button);
}

void TabStripActionContainer::AddedToWidget() {
  views::View::AddedToWidget();
  if (auto* controller =
          tabs::GlicActorNudgeController::From(browser_window_interface_)) {
    controller->UpdateCurrentActorNudgeState();
  }
}

void TabStripActionContainer::MouseMovedOutOfHost() {
  SetLockedExpansionMode(LockedExpansionMode::kNone, nullptr);
}

void TabStripActionContainer::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void TabStripActionContainer::AnimationEnded(const gfx::Animation* animation) {
  animation_session_->ApplyAnimationValue(animation);
  animation_session_->MarkAnimationDone(animation);
  if (glic_button_) {
    glic_button_->OnAnimationEnded();
  }
}

void TabStripActionContainer::OnAnimationSessionEnded() {
  // If the button went from shown -> hidden, unblock the tab strip from
  // showing other modal UIs.
  bool should_reset_modal_ui =
      (animation_session_ &&
       animation_session_->session_type() ==
           TabStripNudgeAnimationSession::AnimationSessionType::kHide);

  // Handle state transitions for the actor task icon, which uses two distinct
  // animation modes:
  // 1. kEntry: The icon animating into existence.
  // 2. kNudge: The label expanding/collapsing.
  if (animation_session_ &&
      animation_session_->button() == glic_actor_task_icon_) {
    const bool is_show =
        animation_session_->session_type() ==
        TabStripNudgeAnimationSession::AnimationSessionType::kShow;
    // Case 1: Entry Animation Finished -> Switch to Nudge Mode
    if (is_show && glic_actor_task_icon_->GetAnimationMode() ==
                       TaskIconAnimationMode::kEntry) {
      glic_actor_task_icon_->SetAnimationMode(TaskIconAnimationMode::kNudge);
      should_reset_modal_ui = true;
    } else if (!is_show && glic_actor_task_icon_->GetAnimationMode() ==
                               TaskIconAnimationMode::kNudge) {
      // Case 2: Nudge Collapse Finished -> Reset Text
      glic_actor_task_icon_->SetIsShowingNudge(false);
      glic_actor_task_icon_->SetTaskIconToDefault();
    } else if (!is_show && glic_actor_task_icon_->GetAnimationMode() ==
                               TaskIconAnimationMode::kEntry) {
      // Case 3: Exit Animation Finished (Hide + kEntry) -> Run Cleanup
      FinalizeHideGlicActorTaskIcon();
      should_reset_modal_ui = true;
    }
  }
  if (should_reset_modal_ui) {
    scoped_tab_strip_modal_ui_.reset();

    if (locked_expansion_button_) {
      locked_expansion_button_->SetIsShowingNudge(false);
    }
  }

  animation_session_.reset();
}

void TabStripActionContainer::AnimationProgressed(
    const gfx::Animation* animation) {
  animation_session_->ApplyAnimationValue(animation);
}

void TabStripActionContainer::UpdateButtonBorders(
    const gfx::Insets border_insets) {
  border_insets_ = border_insets;

  if (glic_button_) {
    UpdateGlicActorButtonContainerBorders();
  }
}

void TabStripActionContainer::SetGlicShowState(bool show) {
  if (glic_button_) {
    glic_button_->SetVisible(show);
  }
  if (separator_) {
    separator_->SetVisible(show);
  }
}

void TabStripActionContainer::SetGlicPanelIsOpen(bool open) {
  if (!glic_button_) {
    return;
  }

  glic_button_->SetGlicPanelIsOpen(open);

  if (base::FeatureList::IsEnabled(features::kGlicButtonPressedState) &&
      features::kGlicButtonContainerBackground.Get()) {
    glic_actor_button_container_->SetBackgroundColor(
        glic_button_->GetBackgroundColor());
    glic_actor_button_container_->SetHighlighted(open);
  }
}

void TabStripActionContainer::DidBecomeActive(BrowserWindowInterface* browser) {
  separator_->SetColorId(kColorTabDividerFrameActive);
}

void TabStripActionContainer::DidBecomeInactive(
    BrowserWindowInterface* browser) {
  separator_->SetColorId(kColorTabDividerFrameInactive);
}

bool TabStripActionContainer::ButtonOwnsAnimation(
    const TabStripNudgeButton* button) const {
  return button == glic_button_ &&
         base::FeatureList::IsEnabled(features::kGlicEntrypointVariations);
}

BEGIN_METADATA(TabStripActionContainer)
END_METADATA
