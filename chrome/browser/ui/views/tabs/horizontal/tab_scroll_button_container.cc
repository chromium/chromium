// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/horizontal/tab_scroll_button_container.h"

#include <algorithm>
#include <cmath>

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/horizontal_tab_strip_metrics.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabScrollButtonContainer,
                                      kTabScrollButtonContainer);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabScrollButtonContainer,
                                      kStartScrollButton);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabScrollButtonContainer,
                                      kEndScrollButton);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabScrollButtonContainer, kUnpinMenuItem);

namespace {
// TODO(b/523328731): Validate animation parameters with design.
constexpr base::TimeDelta kScrollAnimationTime = base::Milliseconds(300);
constexpr int kScrollButtonSpacing = 1;
constexpr int kScrollButtonHorizontalPadding = 4;
constexpr base::TimeDelta kIPHDisplayDelay = base::Seconds(3);
}  // namespace

class TabScrollButtonContainer::TabScrollButtonIPHController
    : public views::ViewObserver {
 public:
  TabScrollButtonIPHController(
      BrowserWindowInterface* browser_window_interface,
      TabScrollButtonContainer* tab_scroll_button_container);
  TabScrollButtonIPHController(const TabScrollButtonIPHController&) = delete;
  TabScrollButtonIPHController& operator=(const TabScrollButtonIPHController&) =
      delete;
  ~TabScrollButtonIPHController() override;

  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view,
                               bool visible) override;

 private:
  void MaybeShowIPH();

  raw_ptr<BrowserWindowInterface> browser_window_interface_ = nullptr;
  raw_ptr<TabScrollButtonContainer> tab_scroll_button_container_ = nullptr;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
  base::OneShotTimer show_iph_timer_;
};

TabScrollButtonContainer::TabScrollButtonIPHController::
    TabScrollButtonIPHController(
        BrowserWindowInterface* browser_window_interface,
        TabScrollButtonContainer* tab_scroll_button_container)
    : browser_window_interface_(browser_window_interface),
      tab_scroll_button_container_(tab_scroll_button_container) {
  CHECK(tab_scroll_button_container_);
  observation_.Observe(tab_scroll_button_container_);
  if (tab_scroll_button_container_->GetVisible()) {
    show_iph_timer_.Start(
        FROM_HERE, kIPHDisplayDelay,
        base::BindOnce(&TabScrollButtonContainer::TabScrollButtonIPHController::
                           MaybeShowIPH,
                       base::Unretained(this)));
  }
}

TabScrollButtonContainer::TabScrollButtonIPHController::
    ~TabScrollButtonIPHController() = default;

void TabScrollButtonContainer::TabScrollButtonIPHController::
    OnViewVisibilityChanged(views::View* observed_view,
                            views::View* starting_view,
                            bool visible) {
  if (!visible) {
    show_iph_timer_.Stop();
    return;
  }

  if (auto* user_education =
          BrowserUserEducationInterface::From(browser_window_interface_)) {
    if (user_education->HasFeaturePromoBeenDismissed(
            feature_engagement::kIPHTabScrollButtonFeature)) {
      show_iph_timer_.Stop();
      observation_.Reset();
      return;
    }
  }

  if (!show_iph_timer_.IsRunning()) {
    show_iph_timer_.Start(
        FROM_HERE, kIPHDisplayDelay,
        base::BindOnce(&TabScrollButtonContainer::TabScrollButtonIPHController::
                           MaybeShowIPH,
                       base::Unretained(this)));
  }
}

void TabScrollButtonContainer::TabScrollButtonIPHController::MaybeShowIPH() {
  if (auto* user_education =
          BrowserUserEducationInterface::From(browser_window_interface_)) {
    user_education->MaybeShowFeaturePromo(
        feature_engagement::kIPHTabScrollButtonFeature);
  }
}

TabScrollButtonContainer::TabScrollButtonContainer(
    BrowserWindowInterface* browser_window_interface)
    : browser_window_interface_(browser_window_interface) {
  set_context_menu_controller(this);
  SetProperty(views::kElementIdentifierKey, kTabScrollButtonContainer);

  std::unique_ptr<views::BoxLayout> box_layout =
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          /*inside_border_insets=*/
          gfx::Insets::VH(0, kScrollButtonHorizontalPadding),
          /*between_child_spacing_=*/kScrollButtonSpacing,
          /*collapse_margins_spacing=*/false);
  box_layout->set_cross_axis_alignment(views::LayoutAlignment::kCenter);
  SetLayoutManager(std::move(box_layout));
  SetMirrored(false);

  start_scroll_button_ = AddChildView(std::make_unique<TabStripControlButton>(
      browser_window_interface,
      views::Button::PressedCallback(base::BindRepeating(
          &TabScrollButtonContainer::BeginScrollAnimation,
          base::Unretained(this), /*scroll_to_start=*/true)),
      kKeyboardArrowLeftIcon, Edge::kNone, Edge::kNone));
  start_scroll_button_->set_context_menu_controller(this);
  start_scroll_button_->SetProperty(views::kElementIdentifierKey,
                                    kStartScrollButton);
  start_scroll_button_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(base::i18n::IsRTL()
                                    ? IDS_ACCNAME_TAB_SCROLL_TRAILING
                                    : IDS_ACCNAME_TAB_SCROLL_LEADING));

  end_scroll_button_ = AddChildView(std::make_unique<TabStripControlButton>(
      browser_window_interface,
      views::Button::PressedCallback(base::BindRepeating(
          &TabScrollButtonContainer::BeginScrollAnimation,
          base::Unretained(this), /*scroll_to_start=*/false)),
      kKeyboardArrowRightIcon, Edge::kNone, Edge::kNone));
  end_scroll_button_->set_context_menu_controller(this);
  end_scroll_button_->SetProperty(views::kElementIdentifierKey,
                                  kEndScrollButton);
  end_scroll_button_->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      base::i18n::IsRTL() ? IDS_ACCNAME_TAB_SCROLL_LEADING
                          : IDS_ACCNAME_TAB_SCROLL_TRAILING));
  start_scroll_button_->SetBorder(views::CreateEmptyBorder(gfx::Insets()));
  end_scroll_button_->SetBorder(views::CreateEmptyBorder(gfx::Insets()));
  animation_.SetDuration(kScrollAnimationTime);

  if (auto* user_education =
          BrowserUserEducationInterface::From(browser_window_interface_)) {
    if (!user_education->HasFeaturePromoBeenDismissed(
            feature_engagement::kIPHTabScrollButtonFeature)) {
      iph_controller_ = std::make_unique<TabScrollButtonIPHController>(
          browser_window_interface_, this);
    }
  }
}

TabScrollButtonContainer::~TabScrollButtonContainer() = default;

void TabScrollButtonContainer::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  context_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);

  context_menu_model_->AddItemWithIcon(
      IDC_TAB_SCROLL_BUTTONS_TOGGLE_PIN,
      l10n_util::GetStringUTF16(IDS_TAB_SCROLL_UNPIN_BUTTONS),
      ui::ImageModel::FromVectorIcon(kKeepOffIcon, ui::kColorIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize));
  context_menu_model_->SetElementIdentifierAt(
      context_menu_model_
          ->GetIndexOfCommandId(IDC_TAB_SCROLL_BUTTONS_TOGGLE_PIN)
          .value(),
      kUnpinMenuItem);

  int32_t menu_runner_flags =
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU;
  context_menu_runner_ = std::make_unique<views::MenuRunner>(
      context_menu_model_.get(), menu_runner_flags);
  context_menu_runner_->RunMenuAt(
      source->GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
      views::MenuAnchorPosition::kTopLeft, source_type);
}

void TabScrollButtonContainer::VisibilityChanged(views::View* starting_from,
                                                 bool is_visible) {
  if (starting_from != this) {
    return;
  }

  if (is_visible) {
    base::RecordAction(
        base::UserMetricsAction("HorizontalTabStrip.ScrollButtons.Visible"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("HorizontalTabStrip.ScrollButtons.Hidden"));
  }
}

void TabScrollButtonContainer::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == IDC_TAB_SCROLL_BUTTONS_TOGGLE_PIN) {
    if (actions::ActionItem* toggle_scroll_pin_action =
            GetToggleScrollPinAction()) {
      base::RecordAction(
          base::UserMetricsAction("TabScrollButton.ContextMenu.Unpinned"));
      toggle_scroll_pin_action->InvokeAction();
    }
  }
}

actions::ActionItem* TabScrollButtonContainer::GetToggleScrollPinAction() {
  if (const BrowserActions* browser_actions =
          BrowserActions::From(browser_window_interface_)) {
    return actions::ActionManager::Get().FindAction(
        kActionTabScrollTogglePin, browser_actions->root_action_item());
  }
  return nullptr;
}

bool TabScrollButtonContainer::IsPositionInWindowCaption(const gfx::Point& p) {
  return !start_scroll_button_->HitTestPoint(
             ConvertPointToTarget(this, start_scroll_button_, p)) &&
         !end_scroll_button_->HitTestPoint(
             ConvertPointToTarget(this, end_scroll_button_, p));
}

void TabScrollButtonContainer::SetScrollView(views::ScrollView* scroll_view) {
  scroll_view_ = scroll_view;

  if (scroll_view == nullptr) {
    animation_.Stop();
  }
}

void TabScrollButtonContainer::BeginScrollAnimation(bool scroll_to_start) {
  CHECK(scroll_view_);
  views::ScrollBar* scroll_bar = scroll_view_->horizontal_scroll_bar();

  if (animation_.is_animating()) {
    animation_.Stop();
  }

  const float page_scroll_amount =
      scroll_view_->GetScrollIncrement(scroll_bar, /*is_page=*/true,
                                       /*is_positive=*/true);
  const float min_inactive_tab_width =
      TabStyle::Get()->GetMinimumInactiveWidth();

  // When pagination scrolling, decrease the scroll distance by the minimum
  // tab width so that at least one of the tabs from before the scroll
  // remains visible.
  const float full_scroll_amount =
      std::max(0.0f, page_scroll_amount - min_inactive_tab_width);
  const float current_offset = scroll_view_->CurrentOffset().x();
  float target_offset = current_offset + (base::i18n::IsRTL() ? -1 : 1) *
                                             (scroll_to_start ? -1 : 1) *
                                             full_scroll_amount;
  target_offset = std::clamp<float>(target_offset, scroll_bar->GetMinPosition(),
                                    scroll_bar->GetMaxPosition());

  if (current_offset == target_offset) {
    animation_params_ = std::nullopt;
    return;
  }

  tabs::RecordHorizontalTabStripScrollSource(
      tabs::HorizontalTabStripScrollSource::kButtons);

  animation_params_ = AnimationParams{.start_offset = current_offset,
                                      .target_offset = target_offset};

  animation_.Start();
}

void TabScrollButtonContainer::AnimationProgressed(
    const gfx::Animation* animation) {
  CHECK(scroll_view_);
  CHECK(animation_params_.has_value());

  float progress = gfx::Tween::CalculateValue(gfx::Tween::Type::EASE_OUT,
                                              animation_.GetCurrentValue());
  const float current_x =
      gfx::Tween::FloatValueBetween(progress, animation_params_->start_offset,
                                    animation_params_->target_offset);
  scroll_view_->ScrollToOffset({current_x, 0.0f});
}

void TabScrollButtonContainer::AnimationEnded(const gfx::Animation* animation) {
  if (scroll_view_) {
    CHECK(animation_params_);
    scroll_view_->ScrollToOffset({animation_params_->target_offset, 0.0f});
  }
  animation_params_ = std::nullopt;
}

void TabScrollButtonContainer::AnimationCanceled(
    const gfx::Animation* animation) {
  animation_params_ = std::nullopt;
}

BEGIN_METADATA(TabScrollButtonContainer)
END_METADATA
