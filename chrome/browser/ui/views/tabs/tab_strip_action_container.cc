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
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/commerce/product_specifications_button.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/commerce/core/commerce_feature_list.h"
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

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager_factory.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/grit/branded_strings.h"
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
constexpr char kAutoTabGroupsTriggerOutcomeName[] =
    "Tab.Organization.Trigger.Outcome";
constexpr char kDeclutterTriggerOutcomeName[] =
    "Tab.Organization.Declutter.Trigger.Outcome";
constexpr char kDeclutterTriggerBucketedCTRName[] =
    "Tab.Organization.Declutter.Trigger.BucketedCTR";

#if BUILDFLAG(ENABLE_GLIC)
constexpr int kLargeSpaceBetweenButtons = 6;
constexpr int kInsideBorderAroundGlicButtons = 2;
constexpr int kOutsideBorderAroundGlicButtons = 11;
#if !BUILDFLAG(IS_MAC)
constexpr int kLargeSpaceBetweenSeparatorRight = 8;
constexpr int kLargeSpaceBetweenSeparatorLeft = 2;
#endif  // !BUILDFLAG(IS_MAC)
#endif  // BUILDFLAG(ENABLE_GLIC)

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
    TabStripController* tab_strip_controller,
    tabs::TabDeclutterController* tab_declutter_controller,
    tabs::GlicNudgeController* glic_nudge_controller)
    : AnimationDelegateViews(this),
      locked_expansion_view_(this),
      tab_declutter_controller_(tab_declutter_controller),
      glic_nudge_controller_(glic_nudge_controller),
      tab_strip_controller_(tab_strip_controller) {
  SetProperty(views::kElementIdentifierKey, kTabStripActionContainerElementId);

  mouse_watcher_ = std::make_unique<views::MouseWatcher>(
      std::make_unique<views::MouseWatcherViewHost>(locked_expansion_view_,
                                                    gfx::Insets()),
      this);

  tab_organization_service_ = TabOrganizationServiceFactory::GetForProfile(
      tab_strip_controller->GetProfile());
  if (tab_organization_service_) {
    tab_organization_observation_.Observe(tab_organization_service_);
  }

  // `tab_declutter_controller_` will be null for some profile types and if
  // feature is not enabled.
  if (tab_declutter_controller_) {
    tab_declutter_button_ =
        AddChildView(CreateTabDeclutterButton(tab_strip_controller));

    SetupButtonProperties(tab_declutter_button_);

    tab_declutter_observation_.Observe(tab_declutter_controller_);
  }

  // `glic_nudge_controller_` will be null if feature is not enabled.
  if (glic_nudge_controller_) {
#if BUILDFLAG(ENABLE_GLIC)
    glic_nudge_controller_->SetDelegate(this);
#else
    NOTREACHED();
#endif  // BUILDFLAG(ENABLE_GLIC)
  }
  auto_tab_group_button_ =
      AddChildView(CreateAutoTabGroupButton(tab_strip_controller));

  SetupButtonProperties(auto_tab_group_button_);

  browser_ = tab_strip_controller->GetBrowser();
  BrowserWindowInterface* browser_window_interface =
      tab_strip_controller->GetBrowserWindowInterface();
  if (base::FeatureList::IsEnabled(commerce::kProductSpecifications)) {
    std::unique_ptr<ProductSpecificationsButton> product_specifications_button;
    product_specifications_button =
        std::make_unique<ProductSpecificationsButton>(
            tab_strip_controller, browser_window_interface->GetTabStripModel(),
            commerce::ProductSpecificationsEntryPointController::From(
                browser_window_interface),
            /*render_tab_search_before_tab_strip_*/ false, this);
    product_specifications_button->SetProperty(views::kCrossAxisAlignmentKey,
                                               views::LayoutAlignment::kCenter);

    product_specifications_button_ =
        AddChildView(std::move(product_specifications_button));
  }

#if BUILDFLAG(ENABLE_GLIC)
  if (glic::GlicEnabling::IsProfileEligible(
          tab_strip_controller->GetProfile())) {
    if (features::kGlicActorUiTaskIcon.Get()) {
      glic_actor_button_container_ =
          AddChildView(CreateGlicActorButtonContainer());
      glic_actor_task_icon_ = glic_actor_button_container_->AddChildView(
          CreateGlicActorTaskIcon(tab_strip_controller));
      glic_actor_button_container_->SetVisible(false);
    }
    glic_button_ = AddChildView(CreateGlicButton(tab_strip_controller));

    SetupButtonProperties(glic_button_);
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

    subscriptions_.push_back(browser_window_interface->RegisterDidBecomeActive(
        base::BindRepeating(&TabStripActionContainer::DidBecomeActive,
                            base::Unretained(this))));
    subscriptions_.push_back(
        browser_window_interface->RegisterDidBecomeInactive(
            base::BindRepeating(&TabStripActionContainer::DidBecomeInactive,
                                base::Unretained(this))));
    separator_ = AddChildView(std::move(separator));
#endif  // !BUILDFLAG(IS_MAC)
  }
#endif  // BUILDFLAG(ENABLE_GLIC)
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

std::unique_ptr<TabStripNudgeButton>
TabStripActionContainer::CreateTabDeclutterButton(
    TabStripController* tab_strip_controller) {
  auto button = std::make_unique<TabStripNudgeButton>(
      tab_strip_controller,
      base::BindRepeating(&TabStripActionContainer::OnTabDeclutterButtonClicked,
                          base::Unretained(this)),
      base::BindRepeating(
          &TabStripActionContainer::OnTabDeclutterButtonDismissed,
          base::Unretained(this)),
      features::IsTabstripDedupeEnabled()
          ? l10n_util::GetStringUTF16(IDS_TAB_DECLUTTER)
          : l10n_util::GetStringUTF16(IDS_TAB_DECLUTTER_NO_DEDUPE),
      kTabDeclutterButtonElementId, Edge::kNone, gfx::VectorIcon::EmptyIcon(),
      /*show_close_button=*/true);

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

std::unique_ptr<TabStripNudgeButton>
TabStripActionContainer::CreateAutoTabGroupButton(
    TabStripController* tab_strip_controller) {
  auto button = std::make_unique<TabStripNudgeButton>(
      tab_strip_controller,
      base::BindRepeating(&TabStripActionContainer::OnAutoTabGroupButtonClicked,
                          base::Unretained(this)),
      base::BindRepeating(
          &TabStripActionContainer::OnAutoTabGroupButtonDismissed,
          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_TAB_ORGANIZE), kAutoTabGroupButtonElementId,
      Edge::kNone, gfx::VectorIcon::EmptyIcon(), /*show_close_button=*/true);
  button->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ORGANIZE));
  button->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_ORGANIZE));
  button->SetProperty(views::kCrossAxisAlignmentKey,
                      views::LayoutAlignment::kCenter);
  return button;
}

#if BUILDFLAG(ENABLE_GLIC)
std::unique_ptr<glic::GlicButton> TabStripActionContainer::CreateGlicButton(
    TabStripController* tab_strip_controller) {
  glic::GlicKeyedService* service =
      glic::GlicKeyedService::Get(tab_strip_controller_->GetProfile());
  std::u16string tooltip_text = l10n_util::GetStringUTF16(
      service->IsWindowOrFreShowing() ? IDS_GLIC_TAB_STRIP_BUTTON_TOOLTIP_CLOSE
                                      : IDS_GLIC_TAB_STRIP_BUTTON_TOOLTIP);
  std::unique_ptr<glic::GlicButton> glic_button =
      std::make_unique<glic::GlicButton>(
          tab_strip_controller,
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

std::unique_ptr<glic::GlicActorTaskIcon>
TabStripActionContainer::CreateGlicActorTaskIcon(
    TabStripController* tab_strip_controller) {
  std::unique_ptr<glic::GlicActorTaskIcon> glic_actor_task_icon =
      std::make_unique<glic::GlicActorTaskIcon>(
          tab_strip_controller,
          base::BindRepeating(
              &TabStripActionContainer::OnGlicActorTaskIconClicked,
              base::Unretained(this)));

  glic_actor_task_icon->SetProperty(views::kCrossAxisAlignmentKey,
                                    views::LayoutAlignment::kCenter);

  return glic_actor_task_icon;
}

// TODO(crbug.com/431015299): Clean up when GlicButton and GlicActorTaskIcon
// have been combined.
std::unique_ptr<views::FlexLayoutView>
TabStripActionContainer::CreateGlicActorButtonContainer() {
  auto glic_actor_button_container = std::make_unique<views::FlexLayoutView>();
  glic_actor_button_container->SetCollapseMargins(true);
  glic_actor_button_container->SetCrossAxisAlignment(
      views::LayoutAlignment::kCenter);
  glic_actor_button_container->SetBackground(views::CreateRoundedRectBackground(
      kColorNewTabButtonCRBackgroundFrameActive, gfx::RoundedCornersF(12),
      gfx::Insets::VH(4, 8)));

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

  const bool is_redesign_enabled =
      base::FeatureList::IsEnabled(features::kGlicActorUiNudgeRedesign);
  // GlicActorTaskIcon will only ever be shown alongside the GlicButton.
  if (glic_actor_task_icon_ && glic_actor_task_icon_->IsDrawn()) {
    gfx::Insets task_icon_border;
    const gfx::Insets right_icon_border = gfx::Insets().set_left_right(
        is_redesign_enabled ? 0 : kInsideBorderAroundGlicButtons,
        kOutsideBorderAroundGlicButtons);
    const gfx::Insets left_icon_border = gfx::Insets().set_left_right(
        kOutsideBorderAroundGlicButtons, kInsideBorderAroundGlicButtons);
    if (is_redesign_enabled) {
      task_icon_border = right_icon_border + border_insets;
      glic_border = left_icon_border + border_insets;
    } else {
      task_icon_border = left_icon_border + border_insets;
      glic_border = right_icon_border + border_insets;
    }
    glic_actor_task_icon_->SetBorder(
        views::CreateEmptyBorder(task_icon_border));
    if (is_redesign_enabled) {
      // Force a background repaint to account for the new border insets.
      glic_actor_task_icon_->RefreshBackground();
    }
  } else {
    // Reset GlicButton border if Task Icon is hidden.
    glic_border = gfx::Insets().set_left_right(border_insets.top(),
                                               border_insets.bottom()) +
                  border_insets;
  }
  glic_button_->SetBorder(views::CreateEmptyBorder(glic_border));
  if (is_redesign_enabled) {
    // Force a background repaint to account for the new border insets.
    glic_button_->RefreshBackground();
  }
}

#endif  // BUILDFLAG(ENABLE_GLIC)

void TabStripActionContainer::OnToggleActionUIState(const Browser* browser,
                                                    bool should_show) {
  CHECK(tab_organization_service_);

  if (locked_expansion_mode_ != LockedExpansionMode::kNone) {
    return;
  }

  if (should_show && browser_ == browser) {
    ShowTabStripNudge(auto_tab_group_button_);
  } else {
    HideTabStripNudge(auto_tab_group_button_);
  }
}

void TabStripActionContainer::OnTabDeclutterButtonClicked() {
  tabs::TabDeclutterController::EmitEntryPointHistogram(
      tab_search::mojom::TabDeclutterEntryPoint::kNudge);
  base::UmaHistogramEnumeration(kDeclutterTriggerOutcomeName,
                                TriggerOutcome::kAccepted);
  LogDeclutterTriggerBucket(true);
  browser_->window()->CreateTabSearchBubble(
      tab_search::mojom::TabSearchSection::kOrganize,
      tab_search::mojom::TabOrganizationFeature::kDeclutter);

  ExecuteHideTabStripNudge(tab_declutter_button_);
}

void TabStripActionContainer::OnTabDeclutterButtonDismissed() {
  base::UmaHistogramEnumeration(kDeclutterTriggerOutcomeName,
                                TriggerOutcome::kDismissed);
  tab_declutter_controller_->OnActionUIDismissed(
      base::PassKey<TabStripActionContainer>());

  ExecuteHideTabStripNudge(tab_declutter_button_);
}

void TabStripActionContainer::OnTriggerDeclutterUIVisibility() {
  CHECK(tab_declutter_controller_);

  ShowTabStripNudge(tab_declutter_button_);
}

#if BUILDFLAG(ENABLE_GLIC)
void TabStripActionContainer::OnGlicButtonClicked() {
  // Indicate that the glic button was pressed so that we can either close the
  // IPH promo (if present) or note that it has already been used to prevent
  // unnecessarily displaying the promo.
  BrowserUserEducationInterface::From(
      tab_strip_controller_->GetBrowserWindowInterface())
      ->NotifyFeaturePromoFeatureUsed(
          feature_engagement::kIPHGlicPromoFeature,
          FeaturePromoFeatureUsedAction::kClosePromoIfPresent);

  std::optional<std::string> prompt_suggestion;
  if (glic_nudge_controller_) {
    prompt_suggestion = glic_nudge_controller_->GetPromptSuggestion();
    glic_nudge_controller_->ClearPromptSuggestion();
  }
  glic::GlicKeyedServiceFactory::GetGlicKeyedService(
      tab_strip_controller_->GetProfile())
      ->ToggleUI(tab_strip_controller_->GetBrowserWindowInterface(),
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
  Profile* profile = tab_strip_controller_->GetProfile();
  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  if (auto* instance = glic_service->GetInstanceForActiveTab(
          tab_strip_controller_->GetBrowserWindowInterface())) {
    instance->host().instance_delegate().PrepareForOpen();
  }
}

void TabStripActionContainer::OnGlicButtonMouseDown() {
  Profile* profile = tab_strip_controller_->GetProfile();
  if (!glic::GlicEnabling::IsEnabledAndConsentForProfile(profile)) {
    // Do not do this optimization if user has not consented to GLIC.
    return;
  }
  auto* glic_service = glic::GlicKeyedService::Get(profile);

  // TODO(crbug.com/445934142): Create the instance here so that suggestions can
  // be fetched, but don't show it yet.
  if (auto* instance = glic_service->GetInstanceForActiveTab(
          tab_strip_controller_->GetBrowserWindowInterface())) {
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
  Profile* profile = tab_strip_controller_->GetProfile();
  auto* icon_manager =
      tabs::GlicActorTaskIconManagerFactory::GetForProfile(profile);
  CHECK(icon_manager);

  if (base::FeatureList::IsEnabled(features::kGlicActorUiNudgeRedesign)) {
    ActorTaskListBubbleController* controller =
        ActorTaskListBubbleController::From(
            tab_strip_controller_->GetBrowserWindowInterface());
    controller->ShowBubble(glic_actor_task_icon_);
    actor::ui::LogTaskNudgeClick(icon_manager->GetCurrentActorTaskNudgeState());
  } else {
    glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile)->ToggleUI(
        tab_strip_controller_->GetBrowserWindowInterface(),
        /*prevent_close=*/false, glic::mojom::InvocationSource::kActorTaskIcon);

    if (glic_actor_task_icon_->GetIsShowingNudge()) {
      icon_manager->ClearStoppedTasks();
      // If a nudge is showing, activate the last actuated tab on click of the
      // Task Icon.
      if (tabs::TabInterface* last_updated_tab =
              icon_manager->GetLastUpdatedTab()) {
        TabStripModel* tab_strip_model =
            tab_strip_controller_->GetBrowserWindowInterface()
                ->GetTabStripModel();
        int tab_index = tab_strip_model->GetIndexOfTab(last_updated_tab);

        // In the case where the tab is currently detached (such as being moved
        // between windows), the tab may not have an index.
        if (tab_index != TabStripModel::kNoTab) {
          tab_strip_model->ActivateTabAt(tab_index);
        }
      }
    }
    actor::ui::LogTaskIconClick();
  }
}

#endif  // BUILDFLAG(ENABLE_GLIC)

void TabStripActionContainer::OnTriggerGlicNudgeUI(std::string label) {
#if BUILDFLAG(ENABLE_GLIC)
  if (GetIsShowingGlicActorTaskIconNudge()) {
    return;
  }

  CHECK(glic_button_);
  if (!label.empty()) {
    glic_button_->SetNudgeLabel(std::move(label));
    ShowTabStripNudge(glic_button_);
  }

#else
  NOTREACHED();
#endif  // BUILDFLAG(ENABLE_GLIC)
}

void TabStripActionContainer::OnHideGlicNudgeUI() {
#if BUILDFLAG(ENABLE_GLIC)

  CHECK(glic_button_);
  HideTabStripNudge(glic_button_);

#else
  NOTREACHED();
#endif  // BUILDFLAG(ENABLE_GLIC)
}

bool TabStripActionContainer::GetIsShowingGlicNudge() {
#if BUILDFLAG(ENABLE_GLIC)
  return glic_button_ && glic_button_->GetIsShowingNudge();
#else
  return false;
#endif  // BUILDFLAG(ENABLE_GLIC)
}

#if BUILDFLAG(ENABLE_GLIC)
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
  // Start animation for clearing text on the glic button.
  glic_button_->SuppressLabel();
  ShowGlicActorTaskIcon();
  glic_actor_task_icon_->ShowNudgeLabel(nudge_text);
  HighlightGlicActorTaskIcon();
  ShowTabStripNudge(glic_actor_task_icon_);
}
#endif  // BUILDFLAG(ENABLE_GLIC)

void TabStripActionContainer::ShowGlicActorTaskIcon() {
#if BUILDFLAG(ENABLE_GLIC)
  CHECK(glic_actor_button_container_);
  CHECK(glic_button_);
  // If the nudge is showing (ex: previous state was CheckTasks), hide the nudge
  // and reset the icon
  if (glic_actor_task_icon_->GetIsShowingNudge()) {
    HideTabStripNudge(glic_actor_task_icon_);
    glic_actor_task_icon_->SetTaskIconToDefault();
  }
  glic_button_ =
      glic_actor_button_container_->AddChildView(std::move(glic_button_));
  // When kGlicActorUiNudgeRedesign is enabled, the GlicButton should be to the
  // left of the GlicActorTaskIcon.
  if (base::FeatureList::IsEnabled(features::kGlicActorUiNudgeRedesign)) {
    glic_actor_task_icon_->SetVisible(true);
    glic_actor_button_container_->ReorderChildView(glic_button_, 0u);
  }

  glic_actor_button_container_->SetVisible(true);
  UpdateGlicActorButtonContainerBorders();
#else
  NOTREACHED();
#endif  // BUILDFLAG(ENABLE_GLIC)
}

void TabStripActionContainer::HideGlicActorTaskIcon() {
#if BUILDFLAG(ENABLE_GLIC)
  CHECK(glic_actor_button_container_);
  CHECK(glic_button_);
  CHECK(glic_actor_task_icon_);

  if (glic_actor_task_icon_->GetIsShowingNudge()) {
    HideTabStripNudge(glic_actor_task_icon_);
    // Once we hide the nudge we want to bring the glic button default label
    // back.
    glic_button_->ShowDefaultLabel();
  }
  glic_actor_task_icon_->SetTaskIconToDefault();
  glic_button_ = AddChildView(std::move(glic_button_));
  glic_actor_button_container_->SetVisible(false);
  UpdateGlicActorButtonContainerBorders();
#if !BUILDFLAG(IS_MAC)
  // Re-add the separator so it's ordered after the GlicButton.
  separator_ = AddChildView(std::move(separator_));
#endif  // !BUILDFLAG(IS_MAC)
#else
  NOTREACHED();
#endif  // BUILDFLAG(ENABLE_GLIC)
}

bool TabStripActionContainer::GetIsShowingGlicActorTaskIconNudge() {
#if BUILDFLAG(ENABLE_GLIC)
  return glic_actor_task_icon_ && glic_actor_task_icon_->GetIsShowingNudge();
#else
  return false;
#endif  // BUILDFLAG(ENABLE_GLIC)
}

void TabStripActionContainer::HighlightGlicActorTaskIcon() {
#if BUILDFLAG(ENABLE_GLIC)
  CHECK(glic_actor_task_icon_);

  glic_actor_task_icon_->HighlightTaskIcon();
#else
  NOTREACHED();
#endif  // BUILDFLAG(ENABLE_GLIC)
}

void TabStripActionContainer::UnhighlightGlicActorTaskIcon() {
#if BUILDFLAG(ENABLE_GLIC)
  CHECK(glic_actor_task_icon_);

  glic_actor_task_icon_->SetDefaultColors();
#else
  NOTREACHED();
#endif  // BUILDFLAG(ENABLE_GLIC)
}

DeclutterTriggerCTRBucket TabStripActionContainer::GetDeclutterTriggerBucket(
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

void TabStripActionContainer::LogDeclutterTriggerBucket(bool clicked) {
  const DeclutterTriggerCTRBucket bucket = GetDeclutterTriggerBucket(clicked);
  base::UmaHistogramEnumeration(kDeclutterTriggerBucketedCTRName, bucket);
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
  if (browser_ && (button == auto_tab_group_button_) &&
      !TabOrganizationUtils::GetInstance()->IsEnabled(browser_->profile())) {
    return;
  }

  // If the tab strip already has a modal UI showing, that is not for the same
  // button being hidden, exit early. If the tab strip has modal UI for the same
  // button being hidden, then continue to reset the animation and start a show
  // animation.
  if (!tab_strip_controller_->CanShowModalUI() &&
      !(animation_session_ &&
        animation_session_->session_type() ==
            TabStripNudgeAnimationSession::AnimationSessionType::kHide &&
        animation_session_->button() == button)) {
    return;
  }

  button->SetIsShowingNudge(true);

#if BUILDFLAG(ENABLE_GLIC)
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
#endif
  scoped_tab_strip_modal_ui_.reset();
  scoped_tab_strip_modal_ui_ = tab_strip_controller_->ShowModalUI();

  if (!ButtonOwnsAnimation(button)) {
    animation_session_ = std::make_unique<TabStripNudgeAnimationSession>(
        button, this,
        TabStripNudgeAnimationSession::AnimationSessionType::kShow,
        base::BindOnce(&TabStripActionContainer::OnAnimationSessionEnded,
                       base::Unretained(this)),
        (button != glic_button_ && button != glic_actor_task_icon_));
    animation_session_->Start();
  }

  if (button == tab_declutter_button_) {
    LogDeclutterTriggerBucket(false);
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
#if BUILDFLAG(ENABLE_GLIC)
  if (button == glic_button_ && button->GetWidthFactor() == 0.0 &&
      !ButtonOwnsAnimation(button)) {
    return;
  }
#endif  // BUILDFLAG(ENABLE_GLIC)
  button->SetIsShowingNudge(false);

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

void TabStripActionContainer::OnAutoTabGroupButtonClicked() {
  base::UmaHistogramEnumeration(kAutoTabGroupsTriggerOutcomeName,
                                TriggerOutcome::kAccepted);
  tab_organization_service_->OnActionUIAccepted(browser_);

  UMA_HISTOGRAM_BOOLEAN("Tab.Organization.AllEntrypoints.Clicked", true);
  UMA_HISTOGRAM_BOOLEAN("Tab.Organization.Proactive.Clicked", true);

  // Force hide the button when pressed, bypassing locked expansion mode.
  ExecuteHideTabStripNudge(auto_tab_group_button_);
}

void TabStripActionContainer::OnAutoTabGroupButtonDismissed() {
  base::UmaHistogramEnumeration(kAutoTabGroupsTriggerOutcomeName,
                                TriggerOutcome::kDismissed);
  tab_organization_service_->OnActionUIDismissed(browser_);

  UMA_HISTOGRAM_BOOLEAN("Tab.Organization.Proactive.Clicked", false);

  // Force hide the button when pressed, bypassing locked expansion mode.
  ExecuteHideTabStripNudge(auto_tab_group_button_);
}

void TabStripActionContainer::OnTabStripNudgeButtonTimeout(
    TabStripNudgeButton* button) {
  if (button == auto_tab_group_button_) {
    base::UmaHistogramEnumeration(kAutoTabGroupsTriggerOutcomeName,
                                  TriggerOutcome::kTimedOut);
    UMA_HISTOGRAM_BOOLEAN("Tab.Organization.Proactive.Clicked", false);
  } else if (button == tab_declutter_button_) {
    base::UmaHistogramEnumeration(kDeclutterTriggerOutcomeName,
                                  TriggerOutcome::kTimedOut);
  }

  // Hide the button if not pressed. Use locked expansion mode to avoid
  // disrupting the user.
  HideTabStripNudge(button);
}

void TabStripActionContainer::SetupButtonProperties(
    TabStripNudgeButton* button) {
  // Set opacity for the button. The glic button beings opaque
  button->SetOpacity(

#if BUILDFLAG(ENABLE_GLIC)
      button == glic_button_ ? 1.0 : 0.0
#else
      0.0
#endif  // BUILDFLAG(ENABLE_GLIC)
  );
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
#if BUILDFLAG(ENABLE_GLIC)
  if (glic_button_) {
    glic_button_->OnAnimationEnded();
  }
#endif
}

void TabStripActionContainer::OnAnimationSessionEnded() {
  // If the button went from shown -> hidden, unblock the tab strip from
  // showing other modal UIs.
  if (animation_session_ &&
      animation_session_->session_type() ==
          TabStripNudgeAnimationSession::AnimationSessionType::kHide) {
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

  if (auto_tab_group_button_) {
    auto_tab_group_button_->SetBorder(views::CreateEmptyBorder(border_insets));
  }
  if (tab_declutter_button_) {
    tab_declutter_button_->SetBorder(views::CreateEmptyBorder(border_insets));
  }
  if (product_specifications_button_) {
    product_specifications_button_->SetBorder(
        views::CreateEmptyBorder(border_insets));
  }
  if (glic_button_) {
#if BUILDFLAG(ENABLE_GLIC)
    UpdateGlicActorButtonContainerBorders();
#else
    NOTREACHED();
#endif  // BUILDFLAG(ENABLE_GLIC)
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
#if BUILDFLAG(ENABLE_GLIC)
  if (glic_button_) {
    glic_button_->SetGlicPanelIsOpen(open);
  }
#endif  // BUILDFLAG(ENABLE_GLIC)
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
#if BUILDFLAG(ENABLE_GLIC)
  return button == glic_button_ &&
         base::FeatureList::IsEnabled(features::kGlicEntrypointVariations);
#else
  return false;
#endif
}

BEGIN_METADATA(TabStripActionContainer)
END_METADATA
