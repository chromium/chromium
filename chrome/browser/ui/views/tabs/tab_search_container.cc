// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_search_container.h"

#include <memory>

#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/mouse_watcher_view_host.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr base::TimeDelta kExpansionInDuration = base::Milliseconds(500);
constexpr base::TimeDelta kExpansionOutDuration = base::Milliseconds(250);
constexpr base::TimeDelta kFlatEdgeInDuration = base::Milliseconds(400);
constexpr base::TimeDelta kFlatEdgeOutDuration = base::Milliseconds(250);
constexpr base::TimeDelta kOpacityInDuration = base::Milliseconds(300);
constexpr base::TimeDelta kOpacityOutDuration = base::Milliseconds(100);
constexpr base::TimeDelta kOpacityDelay = base::Milliseconds(100);
constexpr base::TimeDelta kShowDuration = base::Seconds(16);
constexpr int kSpaceBetweenButtons = 2;

Edge GetFlatEdge(bool is_search_button, bool tab_search_before_chips) {
  const bool is_rtl = base::i18n::IsRTL();
  if ((!is_search_button && tab_search_before_chips) ||
      (is_search_button && !tab_search_before_chips)) {
    return is_rtl ? Edge::kRight : Edge::kLeft;
  }
  return is_rtl ? Edge::kLeft : Edge::kRight;
}

}  // namespace

TabSearchContainer::TabOrganizationAnimationSession::
    TabOrganizationAnimationSession(
        TabStripNudgeButton* button,
        TabSearchContainer* container,
        AnimationSessionType session_type,
        base::OnceCallback<void()> on_animation_ended)
    : button_(button),
      container_(container),
      expansion_animation_(container),
      flat_edge_animation_(container),
      opacity_animation_(container),
      session_type_(session_type),
      on_animation_ended_(std::move(on_animation_ended)) {
  if (session_type_ == AnimationSessionType::HIDE) {
    expansion_animation_.Reset(1);
    flat_edge_animation_.Reset(1);
    opacity_animation_.Reset(1);
  }
}

TabSearchContainer::TabOrganizationAnimationSession::
    ~TabOrganizationAnimationSession() = default;

void TabSearchContainer::TabOrganizationAnimationSession::Start() {
  if (session_type_ ==
      TabOrganizationAnimationSession::AnimationSessionType::SHOW) {
    Show();
  } else {
    Hide();
  }
}

void TabSearchContainer::TabOrganizationAnimationSession::
    ResetExpansionAnimationForTesting(double value) {
  expansion_animation_.Reset(value);
}

void TabSearchContainer::TabOrganizationAnimationSession::
    ResetFlatEdgeAnimationForTesting(double value) {
  flat_edge_animation_.Reset(value);
}

void TabSearchContainer::TabOrganizationAnimationSession::
    ResetOpacityAnimationForTesting(double value) {
  if (opacity_animation_delay_timer_.IsRunning()) {
    opacity_animation_delay_timer_.FireNow();
  }

  opacity_animation_.Reset(value);
}

void TabSearchContainer::TabOrganizationAnimationSession::Show() {
  expansion_animation_.SetTweenType(gfx::Tween::Type::ACCEL_20_DECEL_100);
  opacity_animation_.SetTweenType(gfx::Tween::Type::LINEAR);
  flat_edge_animation_.SetTweenType(gfx::Tween::Type::LINEAR);

  expansion_animation_.SetSlideDuration(
      gfx::Animation::RichAnimationDuration(kExpansionInDuration));
  flat_edge_animation_.SetSlideDuration(
      gfx::Animation::RichAnimationDuration(kFlatEdgeInDuration));
  opacity_animation_.SetSlideDuration(
      gfx::Animation::RichAnimationDuration(kOpacityInDuration));

  expansion_animation_.Show();
  flat_edge_animation_.Show();

  const base::TimeDelta delay =
      gfx::Animation::RichAnimationDuration(kOpacityDelay);
  opacity_animation_delay_timer_.Start(
      FROM_HERE, delay, this,
      &TabSearchContainer::TabOrganizationAnimationSession::
          ShowOpacityAnimation);
}

void TabSearchContainer::TabOrganizationAnimationSession::Hide() {
  // Animate and hide existing chip.
  if (session_type_ ==
      TabOrganizationAnimationSession::AnimationSessionType::SHOW) {
    if (opacity_animation_delay_timer_.IsRunning()) {
      opacity_animation_delay_timer_.FireNow();
    }
    session_type_ = TabOrganizationAnimationSession::AnimationSessionType::HIDE;
  }

  expansion_animation_.SetTweenType(gfx::Tween::Type::ACCEL_20_DECEL_100);
  opacity_animation_.SetTweenType(gfx::Tween::Type::LINEAR);
  flat_edge_animation_.SetTweenType(gfx::Tween::Type::ACCEL_20_DECEL_100);

  expansion_animation_.SetSlideDuration(
      gfx::Animation::RichAnimationDuration(kExpansionOutDuration));

  flat_edge_animation_.SetSlideDuration(
      gfx::Animation::RichAnimationDuration(kFlatEdgeOutDuration));

  opacity_animation_.SetSlideDuration(
      gfx::Animation::RichAnimationDuration(kOpacityOutDuration));

  expansion_animation_.Hide();
  flat_edge_animation_.Hide();
  opacity_animation_.Hide();
}

void TabSearchContainer::TabOrganizationAnimationSession::
    ShowOpacityAnimation() {
  opacity_animation_.Show();
}

void TabSearchContainer::TabOrganizationAnimationSession::ApplyAnimationValue(
    const gfx::Animation* animation) {
  float value = animation->GetCurrentValue();
  if (animation == &expansion_animation_) {
    button_->SetWidthFactor(value);
  } else if (animation == &flat_edge_animation_) {
    container_->tab_search_button()->SetFlatEdgeFactor(1 - value);
    button_->SetFlatEdgeFactor(1 - value);
  } else if (animation == &opacity_animation_) {
    button_->SetOpacity(value);
  }
}

void TabSearchContainer::TabOrganizationAnimationSession::MarkAnimationDone(
    const gfx::Animation* animation) {
  if (animation == &expansion_animation_) {
    expansion_animation_done_ = true;
  } else if (animation == &flat_edge_animation_) {
    flat_edge_animation_done_ = true;
  } else {
    opacity_animation_done_ = true;
  }

  if (expansion_animation_done_ && flat_edge_animation_done_ &&
      opacity_animation_done_) {
    if (on_animation_ended_) {
      std::move(on_animation_ended_).Run();
    }
  }
}

TabSearchContainer::TabSearchContainer(bool tab_search_before_chips,
                                       View* locked_expansion_view,
                                       TabStrip* tab_strip)
    : AnimationDelegateViews(this),
      locked_expansion_view_(locked_expansion_view) {
  TabStripController* const tab_strip_controller = tab_strip->controller();
  browser_window_interface_ = tab_strip_controller->GetBrowserWindowInterface();
  SetProperty(views::kElementIdentifierKey, kTabSearchContainerElementId);
  mouse_watcher_ = std::make_unique<views::MouseWatcher>(
      std::make_unique<views::MouseWatcherViewHost>(locked_expansion_view,
                                                    gfx::Insets()),
      this);

  tab_organization_service_ = TabOrganizationServiceFactory::GetForProfile(
      browser_window_interface_->GetProfile());
  if (tab_organization_service_) {
    tab_organization_observation_.Observe(tab_organization_service_);
  }

  // Edge adjacent to new tab button should be rounded and opposite edge
  // should animate to flat on chip show.
  std::unique_ptr<TabSearchButton> tab_search_button =
      std::make_unique<TabSearchButton>(
          browser_window_interface_, Edge::kNone,
          GetFlatEdge(true, tab_search_before_chips));
  tab_search_button->SetProperty(views::kCrossAxisAlignmentKey,
                                 views::LayoutAlignment::kCenter);
  tab_search_button_ = AddChildView(std::move(tab_search_button));

  int tab_search_button_index = GetIndexOf(tab_search_button_).value();
  int index = tab_search_before_chips ? tab_search_button_index + 1
                                      : tab_search_button_index;
  // TODO(crbug.com/40925230): Consider hiding the button when the request has
  // started, vs. when the button as clicked.
  auto_tab_group_button_ =
      AddChildViewAt(CreateAutoTabGroupButton(tab_search_before_chips), index);

  SetupButtonProperties(auto_tab_group_button_, tab_search_before_chips);

  // `tab_declutter_controller` will be null for some profile types and if
  // feature is not enabled.
  tabs::TabDeclutterController* tab_declutter_controller =
      browser_window_interface_->GetFeatures().tab_declutter_controller();
  if (tab_declutter_controller) {
    tab_declutter_button_ = AddChildViewAt(
        CreateTabDeclutterButton(tab_search_before_chips), index);

    SetupButtonProperties(tab_declutter_button_, tab_search_before_chips);

    tab_declutter_observation_.Observe(tab_declutter_controller);
  }

  SetLayoutManager(std::make_unique<views::FlexLayout>());
}

TabSearchContainer::~TabSearchContainer() {
  if (scoped_tab_strip_modal_ui_) {
    scoped_tab_strip_modal_ui_.reset();
  }
}

void TabSearchContainer::SetupButtonProperties(TabStripNudgeButton* button,
                                               bool tab_search_before_chips) {
  // Set the margins for the button
  gfx::Insets margin;
  if (tab_search_before_chips) {
    margin.set_left(kSpaceBetweenButtons);
  } else {
    margin.set_right(kSpaceBetweenButtons);
  }
  button->SetProperty(views::kMarginsKey, margin);

  // Set opacity for the button
  button->SetOpacity(0);
}

std::unique_ptr<TabStripNudgeButton>
TabSearchContainer::CreateAutoTabGroupButton(bool tab_search_before_chips) {
  auto button = std::make_unique<TabStripNudgeButton>(
      browser_window_interface_,
      base::BindRepeating(&TabSearchContainer::OnAutoTabGroupButtonClicked,
                          base::Unretained(this)),
      base::BindRepeating(&TabSearchContainer::OnAutoTabGroupButtonDismissed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_TAB_ORGANIZE), kAutoTabGroupButtonElementId,
      GetFlatEdge(false, tab_search_before_chips), gfx::VectorIcon::EmptyIcon(),
      /*show_close_button=*/true);
  button->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ORGANIZE));
  button->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_ORGANIZE));
  button->SetProperty(views::kCrossAxisAlignmentKey,
                      views::LayoutAlignment::kCenter);
  return button;
}

std::unique_ptr<TabStripNudgeButton>
TabSearchContainer::CreateTabDeclutterButton(
    bool tab_search_before_chips) {
  auto button = std::make_unique<TabStripNudgeButton>(
      browser_window_interface_,
      base::BindRepeating(&TabSearchContainer::OnTabDeclutterButtonClicked,
                          base::Unretained(this)),
      base::BindRepeating(&TabSearchContainer::OnTabDeclutterButtonDismissed,
                          base::Unretained(this)),
      features::IsTabstripDedupeEnabled()
          ? l10n_util::GetStringUTF16(IDS_TAB_DECLUTTER)
          : l10n_util::GetStringUTF16(IDS_TAB_DECLUTTER_NO_DEDUPE),
      kTabDeclutterButtonElementId, GetFlatEdge(false, tab_search_before_chips),
      gfx::VectorIcon::EmptyIcon(), /*show_close_button=*/true);

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

void TabSearchContainer::ShowTabOrganization(TabStripNudgeButton* button) {
  if (locked_expansion_view_->IsMouseHovered()) {
    SetLockedExpansionMode(LockedExpansionMode::kWillShow, button);
  }
  if (locked_expansion_mode_ == LockedExpansionMode::kNone) {
    ExecuteShowTabOrganization(button);
  }
}

void TabSearchContainer::HideTabOrganization(TabStripNudgeButton* button) {
  if (locked_expansion_view_->IsMouseHovered()) {
    SetLockedExpansionMode(LockedExpansionMode::kWillHide, button);
  }
  if (locked_expansion_mode_ == LockedExpansionMode::kNone) {
    ExecuteHideTabOrganization(button);
  }
}

void TabSearchContainer::SetLockedExpansionModeForTesting(
    LockedExpansionMode mode,
    TabStripNudgeButton* button) {
  SetLockedExpansionMode(mode, button);
}

void TabSearchContainer::OnAutoTabGroupButtonClicked() {
  tab_organization_service_->OnActionUIAccepted(
      browser_window_interface_->GetBrowserForMigrationOnly());

  // Force hide the button when pressed, bypassing locked expansion mode.
  ExecuteHideTabOrganization(auto_tab_group_button_);
}

void TabSearchContainer::OnAutoTabGroupButtonDismissed() {
  tab_organization_service_->OnActionUIDismissed(
      browser_window_interface_->GetBrowserForMigrationOnly());

  // Force hide the button when pressed, bypassing locked expansion mode.
  ExecuteHideTabOrganization(auto_tab_group_button_);
}

void TabSearchContainer::OnOrganizeButtonTimeout(TabStripNudgeButton* button) {
  // Hide the button if not pressed. Use locked expansion mode to avoid
  // disrupting the user.
  HideTabOrganization(button);
}

void TabSearchContainer::SetLockedExpansionMode(LockedExpansionMode mode,
                                                TabStripNudgeButton* button) {
  if (mode == LockedExpansionMode::kNone) {
    if (locked_expansion_mode_ == LockedExpansionMode::kWillShow) {
      ExecuteShowTabOrganization(locked_expansion_button_);
    } else if (locked_expansion_mode_ == LockedExpansionMode::kWillHide) {
      ExecuteHideTabOrganization(locked_expansion_button_);
    }
    locked_expansion_button_ = nullptr;
  } else {
    locked_expansion_button_ = button;
    mouse_watcher_->Start(GetWidget()->GetNativeWindow());
  }
  locked_expansion_mode_ = mode;
}

void TabSearchContainer::ExecuteShowTabOrganization(
    TabStripNudgeButton* button) {
  if (browser_window_interface_ && (button == auto_tab_group_button_) &&
      !TabOrganizationUtils::GetInstance()->IsEnabled(
          browser_window_interface_->GetProfile())) {
    return;
  }

  TabStripModel* const tab_strip_model =
      browser_window_interface_->GetTabStripModel();
  // If the tab strip already has a modal UI showing, exit early.
  if (!tab_strip_model->CanShowModalUI()) {
    return;
  }

  scoped_tab_strip_modal_ui_ = tab_strip_model->ShowModalUI();

  animation_session_ = std::make_unique<TabOrganizationAnimationSession>(
      button, this, TabOrganizationAnimationSession::AnimationSessionType::SHOW,
      base::BindOnce(&TabSearchContainer::OnAnimationSessionEnded,
                     base::Unretained(this)));
  animation_session_->Start();

  hide_tab_organization_timer_.Start(
      FROM_HERE, kShowDuration,
      base::BindOnce(&TabSearchContainer::OnOrganizeButtonTimeout,
                     base::Unretained(this), button));
}

void TabSearchContainer::ExecuteHideTabOrganization(
    TabStripNudgeButton* button) {
  // Hide the current animation if the shown button is the same button. Do not
  // create a new animation session.
  if (animation_session_ &&
      animation_session_->session_type() ==
          TabOrganizationAnimationSession::AnimationSessionType::SHOW &&
      animation_session_->button() == button) {
    hide_tab_organization_timer_.Stop();
    animation_session_->Hide();
    return;
  }

  if (!button->GetVisible()) {
    return;
  }

  // Stop the timer since the chip might be getting hidden on user actions like
  // dismissal or click and not timeout.
  hide_tab_organization_timer_.Stop();
  animation_session_ = std::make_unique<TabOrganizationAnimationSession>(
      button, this, TabOrganizationAnimationSession::AnimationSessionType::HIDE,
      base::BindOnce(&TabSearchContainer::OnAnimationSessionEnded,
                     base::Unretained(this)));
  animation_session_->Start();
}

void TabSearchContainer::MouseMovedOutOfHost() {
  SetLockedExpansionMode(LockedExpansionMode::kNone, nullptr);
}

void TabSearchContainer::AnimationCanceled(const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void TabSearchContainer::AnimationEnded(const gfx::Animation* animation) {
  animation_session_->ApplyAnimationValue(animation);
  animation_session_->MarkAnimationDone(animation);
}

void TabSearchContainer::OnAnimationSessionEnded() {
  // If the button went from shown -> hidden, unblock the tab strip from
  // showing other modal UIs.
  if (animation_session_->session_type() ==
      TabOrganizationAnimationSession::AnimationSessionType::HIDE) {
    scoped_tab_strip_modal_ui_.reset();
  }

  animation_session_.reset();
}

void TabSearchContainer::AnimationProgressed(const gfx::Animation* animation) {
  animation_session_->ApplyAnimationValue(animation);
}

void TabSearchContainer::OnToggleActionUIState(const Browser* browser,
                                               bool should_show) {
  CHECK(tab_organization_service_);

  if (locked_expansion_mode_ != LockedExpansionMode::kNone) {
    return;
  }

  if (should_show && browser_window_interface_ == browser) {
    ShowTabOrganization(auto_tab_group_button_);
  } else {
    HideTabOrganization(auto_tab_group_button_);
  }
}

void TabSearchContainer::OnTabDeclutterButtonClicked() {
  BrowserView::GetBrowserViewForBrowser(browser_window_interface_)
      ->CreateTabSearchBubble(
          tab_search::mojom::TabSearchSection::kOrganize,
          tab_search::mojom::TabOrganizationFeature::kDeclutter);

  // Force hide the button when pressed, bypassing locked expansion mode.
  ExecuteHideTabOrganization(tab_declutter_button_);
}

void TabSearchContainer::OnTabDeclutterButtonDismissed() {
  tabs::TabDeclutterController* const tab_declutter_controller =
      browser_window_interface_->GetFeatures().tab_declutter_controller();
  tab_declutter_controller->OnActionUIDismissed(
      base::PassKey<TabSearchContainer>());

  // Force hide the button when pressed, bypassing locked expansion mode.
  ExecuteHideTabOrganization(tab_declutter_button_);
}

void TabSearchContainer::OnTriggerDeclutterUIVisibility() {
  tabs::TabDeclutterController* const tab_declutter_controller =
      browser_window_interface_->GetFeatures().tab_declutter_controller();
  CHECK(tab_declutter_controller);
  if (locked_expansion_mode_ != LockedExpansionMode::kNone) {
    return;
  }

  ShowTabOrganization(tab_declutter_button_);
}

BEGIN_METADATA(TabSearchContainer)
END_METADATA
