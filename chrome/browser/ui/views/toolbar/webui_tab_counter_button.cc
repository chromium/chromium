// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_tab_counter_button.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/i18n/message_formatter.h"
#include "base/i18n/number_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/flying_indicator.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/multi_animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget.h"

namespace {

// The distance to move a label so it appears "offscreen" - that is, the text
// will be clipped by the border and not visible.
constexpr int kOffscreenLabelDistance = 16;

constexpr base::TimeDelta kFirstPartDuration = base::Milliseconds(100);

// Returns whether |change| to |tab_strip_mode| should start the tab counter
// throbber animation.
bool ShouldChangeStartThrobber(TabStripModel* tab_strip_model,
                               const TabStripModelChange& change) {
  if (change.type() != TabStripModelChange::kInserted)
    return false;
  const auto& contents = change.GetInsert()->contents;
  return contents.size() > 1 ||
         tab_strip_model->GetActiveWebContents() != contents[0].contents;
}

std::u16string GetTabCounterLabelText(int num_tabs) {
  // In the triple-digit case, fall back to ':D' to match Android.
  if (num_tabs >= 100)
    return std::u16string(u":D");
  return base::FormatNumber(num_tabs);
}

//------------------------------------------------------------------------
// NumberLabel

// Label to display a number of tabs. Because there is limited space within the
// tab counter border, the font shrinks when the count is 10 or higher.
class NumberLabel : public views::Label {
  METADATA_HEADER(NumberLabel, views::Label)

 public:
  NumberLabel() : Label(std::u16string(), CONTEXT_TAB_COUNTER) {
    single_digit_font_ = font_list();
    double_digit_font_ = views::TypographyProvider::Get().GetFont(
        CONTEXT_TAB_COUNTER, views::style::STYLE_SECONDARY);
  }

  ~NumberLabel() override = default;

  void SetText(const std::u16string& text) override {
    SetFontList(text.length() > 1 ? double_digit_font_ : single_digit_font_);
    Label::SetText(text);
  }

 private:
  gfx::FontList single_digit_font_;
  gfx::FontList double_digit_font_;
};

BEGIN_METADATA(NumberLabel)
END_METADATA

///////////////////////////////////////////////////////////////////////////////
// InteractionTracker

// Listens in on the widget event stream (as a pre target event handler) and
// records user interactions (mouse clicks, taps, etc.) Used so that we know
// where a link that was opened in a background tab was opened from so that we
// can play a "flying link" animation.
class InteractionTracker : public ui::EventHandler,
                           public views::WidgetObserver {
 public:
  explicit InteractionTracker(views::Widget* widget)
      : native_window_(widget->GetNativeWindow()) {
    if (native_window_)
      native_window_->AddPreTargetHandler(this);
    scoped_widget_observation_.Observe(widget);
  }

  InteractionTracker(const InteractionTracker& other) = delete;
  void operator=(const InteractionTracker& other) = delete;

  ~InteractionTracker() override {
    if (native_window_)
      native_window_->RemovePreTargetHandler(this);
  }

  const std::optional<gfx::Point>& last_interaction_location() const {
    return last_interaction_location_;
  }

 private:
  // ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    if (event->type() == ui::EventType::kMousePressed ||
        event->type() == ui::EventType::kMouseReleased ||
        event->type() == ui::EventType::kTouchPressed) {
      const ui::LocatedEvent* const located = event->AsLocatedEvent();
      last_interaction_location_ =
          located->target()->GetScreenLocation(*located);
    }
  }

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override {
    last_interaction_location_.reset();
  }
  void OnWidgetDestroying(views::Widget* widget) override {
    // Clean up all of our observers and event handlers before the native window
    // disappears.
    DCHECK(scoped_widget_observation_.IsObservingSource(widget));
    scoped_widget_observation_.Reset();
    if (widget->GetNativeWindow()) {
      widget->GetNativeWindow()->RemovePreTargetHandler(this);
      native_window_ = nullptr;
    }
  }

  std::optional<gfx::Point> last_interaction_location_;
  gfx::NativeWindow native_window_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      scoped_widget_observation_{this};
};

//------------------------------------------------------------------------
// TabCounterAnimator

// Animates the label and border. |border_view_| does a little bounce. At the
// peak of |border_view_|'s bounce, the |disappearing_label_| begins to scroll
// away in the same direction and is replaced with |appearing_label_|, which
// shows the new number of tabs. This animation is played upside-down when a tab
// is added vs. removed.
class TabCounterAnimator : public gfx::AnimationDelegate {
 public:
  TabCounterAnimator(views::Label* appearing_label,
                     views::Label* disappearing_label,
                     views::View* border_view,
                     views::Throbber* throbber);
  TabCounterAnimator(const TabCounterAnimator&) = delete;
  void operator=(const TabCounterAnimator&) = delete;
  ~TabCounterAnimator() override = default;

  void Animate(int new_num_tabs, bool should_start_throbber);
  void StartFlyingLinkFrom(const gfx::Point& screen_position);
  void LayoutIfAnimating();

 private:
  // Describes the current counter animation (if any). The animation is played
  // one way to show a decrease, and upside down from that to show an increase.
  enum class TabCounterAnimationType { kNone, kIncreasing, kDecreasing };

  // AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  void MaybeStartPendingAnimation();
  void StartAnimation();

  int GetBorderTargetYDelta() const;
  int GetBorderOvershootYDelta() const;
  int GetAppearingLabelStartPosition() const;
  int GetDisappearingLabelTargetPosition() const;
  int GetBorderStartingY() const;

  std::optional<int> last_num_tabs_;
  std::optional<int> pending_num_tabs_ = 0;
  bool pending_throbber_ = false;
  TabCounterAnimationType current_animation_ = TabCounterAnimationType::kNone;

  // The label that will be animated into view, showing the new value.
  const raw_ptr<views::Label> appearing_label_;
  // The label that will be animated out of view, showing the old value.
  const raw_ptr<views::Label> disappearing_label_;
  gfx::MultiAnimation label_animation_;

  const raw_ptr<views::View> border_view_;
  gfx::MultiAnimation border_animation_;

  const raw_ptr<views::Throbber> throbber_;
  base::OneShotTimer throbber_timer_;

  std::unique_ptr<FlyingIndicator> flying_link_;
};

TabCounterAnimator::TabCounterAnimator(views::Label* appearing_label,
                                       views::Label* disappearing_label,
                                       views::View* border_view,
                                       views::Throbber* throbber)
    : appearing_label_(appearing_label),
      disappearing_label_(disappearing_label),
      label_animation_(std::vector<gfx::MultiAnimation::Part>{
          // Stay in place.
          gfx::MultiAnimation::Part(kFirstPartDuration, gfx::Tween::Type::ZERO),
          // Swap out to the new label.
          gfx::MultiAnimation::Part(base::Milliseconds(200),
                                    gfx::Tween::Type::EASE_IN_OUT)}),
      border_view_(border_view),
      border_animation_(std::vector<gfx::MultiAnimation::Part>{
          gfx::MultiAnimation::Part(kFirstPartDuration,
                                    gfx::Tween::Type::EASE_OUT),
          gfx::MultiAnimation::Part(base::Milliseconds(150),
                                    gfx::Tween::Type::EASE_IN_OUT),
          gfx::MultiAnimation::Part(base::Milliseconds(50),
                                    gfx::Tween::Type::EASE_IN_OUT)}),
      throbber_(throbber) {
  label_animation_.set_delegate(this);
  label_animation_.set_continuous(false);

  border_animation_.set_delegate(this);
  border_animation_.set_continuous(false);
}

void TabCounterAnimator::Animate(int new_num_tabs, bool should_start_throbber) {
  pending_num_tabs_ = new_num_tabs;
  pending_throbber_ |= should_start_throbber;
  MaybeStartPendingAnimation();
}

void TabCounterAnimator::MaybeStartPendingAnimation() {
  if (flying_link_ && flying_link_->is_flying())
    return;

  if (pending_throbber_) {
    // Start the throbber if it is not already showing.
    if (!throbber_timer_.IsRunning())
      throbber_->Start();

    // Automatically stop the throbber after 1 second. This will reset the timer
    // if it is already running. Currently we do not check the real loading
    // state of the new tab(s), as that adds unnecessary complexity. The purpose
    // of the throbber is just to indicate to the user that some activity has
    // happened in the background, which may not otherwise have been obvious
    // because the tab strip is hidden in this mode.
    throbber_timer_.Start(FROM_HERE, base::Milliseconds(1000), throbber_.get(),
                          &views::Throbber::Stop);

    pending_throbber_ = false;
  }

  if (pending_num_tabs_.has_value()) {
    if (last_num_tabs_.has_value() &&
        last_num_tabs_.value() != pending_num_tabs_.value()) {
      current_animation_ = pending_num_tabs_.value() > last_num_tabs_.value()
                               ? TabCounterAnimationType::kIncreasing
                               : TabCounterAnimationType::kDecreasing;
      disappearing_label_->SetText(appearing_label_->GetText());
      appearing_label_->SetText(
          GetTabCounterLabelText(pending_num_tabs_.value()));
      border_animation_.Stop();
      border_animation_.Start();
      label_animation_.Stop();
      label_animation_.Start();
      appearing_label_->InvalidateLayout();
      LayoutIfAnimating();
    } else if (!last_num_tabs_.has_value()) {
      appearing_label_->SetText(
          GetTabCounterLabelText(pending_num_tabs_.value()));
    }
    last_num_tabs_ = pending_num_tabs_;
  }
}

void TabCounterAnimator::StartFlyingLinkFrom(
    const gfx::Point& screen_position) {
  flying_link_ = FlyingIndicator::StartFlyingIndicator(
      kWebIcon, screen_position, throbber_,
      base::BindOnce(&TabCounterAnimator::MaybeStartPendingAnimation,
                     base::Unretained(this)));
}

void TabCounterAnimator::LayoutIfAnimating() {
  if (!border_animation_.is_animating() && !label_animation_.is_animating())
    return;

  // |border_view_| does a hop or a dip based on animation type.
  int border_y_delta = 0;
  switch (border_animation_.current_part_index()) {
    case 0:
      // Move away.
      border_y_delta = gfx::Tween::IntValueBetween(
          border_animation_.GetCurrentValue(), 0, GetBorderTargetYDelta());
      break;
    case 1:
      // Return, slightly overshooting the start position.
      border_y_delta = gfx::Tween::IntValueBetween(
          border_animation_.GetCurrentValue(), GetBorderTargetYDelta(),
          GetBorderOvershootYDelta());
      break;
    case 2:
      // Return back to the start position.
      border_y_delta = gfx::Tween::IntValueBetween(
          border_animation_.GetCurrentValue(), GetBorderOvershootYDelta(), 0);
      break;
    default:
      NOTREACHED();
  }
  border_view_->SetY(GetBorderStartingY() + border_y_delta);

  // |appearing_label_| scrolls into view - from above if the counter is
  // increasing, below if it is decreasing.
  const int appearing_label_position = gfx::Tween::IntValueBetween(
      label_animation_.GetCurrentValue(), GetAppearingLabelStartPosition(), 0);
  appearing_label_->SetY(appearing_label_position - border_y_delta);

  // |disappearing_label_| scrolls out of view - out the bottom if
  // |appearing_label_| is decreasing, and from below if increasing.
  const int disappearing_label_position =
      gfx::Tween::IntValueBetween(label_animation_.GetCurrentValue(), 0,
                                  GetDisappearingLabelTargetPosition());
  disappearing_label_->SetY(disappearing_label_position - border_y_delta);
}

void TabCounterAnimator::AnimationProgressed(const gfx::Animation* animation) {
  LayoutIfAnimating();
}

void TabCounterAnimator::AnimationEnded(const gfx::Animation* animation) {
  AnimationProgressed(animation);
}

int TabCounterAnimator::GetBorderTargetYDelta() const {
  constexpr int kBorderBounceDistance = 4;
  switch (current_animation_) {
    case TabCounterAnimationType::kIncreasing:
      return kBorderBounceDistance;
    case TabCounterAnimationType::kDecreasing:
      return -kBorderBounceDistance;
    default:
      NOTREACHED();
  }
}

int TabCounterAnimator::GetBorderOvershootYDelta() const {
  constexpr int kBorderBounceOvershoot = 2;
  switch (current_animation_) {
    case TabCounterAnimationType::kIncreasing:
      return -kBorderBounceOvershoot;
    case TabCounterAnimationType::kDecreasing:
      return kBorderBounceOvershoot;
    default:
      NOTREACHED();
  }
}

int TabCounterAnimator::GetAppearingLabelStartPosition() const {
  switch (current_animation_) {
    case TabCounterAnimationType::kIncreasing:
      return -kOffscreenLabelDistance;
    case TabCounterAnimationType::kDecreasing:
      return kOffscreenLabelDistance;
    default:
      NOTREACHED();
  }
}

int TabCounterAnimator::GetDisappearingLabelTargetPosition() const {
  // We want to exit out the opposite side that |appearing_label_| entered
  // from.
  return -GetAppearingLabelStartPosition();
}

int TabCounterAnimator::GetBorderStartingY() const {
  // When at rest, |border_view_| should be vertically centered within its
  // container.
  views::View* border_container = border_view_->parent();
  int border_available_space = border_container->GetLocalBounds().height();
  return (border_available_space - border_view_->GetLocalBounds().height()) / 2;
}

//------------------------------------------------------------------------
// WebUITabCounterButton

class WebUITabCounterButton : public views::Button,
                              public TabStripModelObserver,
                              public views::ContextMenuController,
                              public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(WebUITabCounterButton, views::Button)

 public:
  static constexpr int WEBUI_TAB_COUNTER_CXMENU_CLOSE_TAB = 13;
  static constexpr int WEBUI_TAB_COUNTER_CXMENU_NEW_TAB = 14;

  WebUITabCounterButton(PressedCallback pressed_callback,
                        BrowserView* browser_view);
  ~WebUITabCounterButton() override;

  void UpdateTooltip(int tab_count);
  void UpdateColors();
  void Init();

 private:
  // views::Button:
  void AddedToWidget() override;
  void AfterPropertyChange(const void* key, int64_t old_value) override;
  void AddLayerToRegion(ui::Layer* new_layer,
                        views::LayerRegion region) override;
  void RemoveLayerFromRegions(ui::Layer* old_layer) override;
  void OnThemeChanged() override;
  void Layout(PassKey) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  void MaybeStartFlyingLink(WindowOpenDisposition disposition);

  raw_ptr<views::InkDropContainerView> ink_drop_container_;
  raw_ptr<views::Label> appearing_label_;
  raw_ptr<views::Label> disappearing_label_;
  raw_ptr<views::View> border_view_;
  std::unique_ptr<TabCounterAnimator> animator_;
  raw_ptr<views::Throbber> throbber_;

  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  std::unique_ptr<InteractionTracker> interaction_tracker_;

  const raw_ptr<TabStripModel> tab_strip_model_;
  const raw_ptr<BrowserView> browser_view_;
  base::CallbackListSubscription link_opened_from_gesture_subscription_;
};

WebUITabCounterButton::WebUITabCounterButton(PressedCallback pressed_callback,
                                             BrowserView* browser_view)
    : Button(std::move(pressed_callback)),
      tab_strip_model_(browser_view->browser()->tab_strip_model()),
      browser_view_(browser_view) {
  ConfigureInkDropForToolbar(this);
  // Not focusable by default, only for accessibility.
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetProperty(views::kElementIdentifierKey, kToolbarTabCounterButtonElementId);
}

WebUITabCounterButton::~WebUITabCounterButton() {
  // TODO(pbos): Revisit explicit removal of InkDrop for classes that override
  // Add/RemoveLayerFromRegionsw(). This is done so that the InkDrop doesn't
  // access the non-override versions in ~View.
  views::InkDrop::Remove(this);
}

void WebUITabCounterButton::UpdateTooltip(int num_tabs) {
  SetTooltipText(base::i18n::MessageFormatter::FormatWithNumberedArgs(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_WEBUI_TAB_STRIP_TAB_COUNTER),
      num_tabs));
}

void WebUITabCounterButton::UpdateColors() {
  const auto* const color_provider = GetColorProvider();
  const SkColor toolbar_color = color_provider->GetColor(kColorToolbar);
  appearing_label_->SetBackgroundColor(toolbar_color);
  disappearing_label_->SetBackgroundColor(toolbar_color);

  const SkColor normal_text_color =
      color_provider->GetColor(kColorToolbarButtonIcon);
  const SkColor current_text_color =
      GetProperty(user_education::kHasInProductHelpPromoKey)
          ? color_provider->GetColor(kColorToolbarFeaturePromoHighlight)
          : normal_text_color;

  appearing_label_->SetEnabledColor(current_text_color);
  disappearing_label_->SetEnabledColor(current_text_color);
  border_view_->SetBorder(views::CreateRoundedRectBorder(
      2,
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kMedium),
      current_text_color));
}

void WebUITabCounterButton::Init() {
  SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(1));

  const int button_height = GetLayoutConstant(TOOLBAR_BUTTON_HEIGHT);
  SetPreferredSize(gfx::Size(button_height, button_height));

  ink_drop_container_ =
      AddChildView(std::make_unique<views::InkDropContainerView>());

  throbber_ = AddChildView(std::make_unique<views::Throbber>());
  throbber_->SetCanProcessEventsWithinSubtree(false);

  border_view_ = AddChildView(std::make_unique<views::View>());
  border_view_->SetCanProcessEventsWithinSubtree(false);

  appearing_label_ =
      border_view_->AddChildView(std::make_unique<NumberLabel>());
  disappearing_label_ =
      border_view_->AddChildView(std::make_unique<NumberLabel>());

  animator_ = std::make_unique<TabCounterAnimator>(
      appearing_label_, disappearing_label_, border_view_, throbber_);

  set_context_menu_controller(this);
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model_->AddItemWithIcon(
      WEBUI_TAB_COUNTER_CXMENU_CLOSE_TAB,
      l10n_util::GetStringUTF16(
          IDS_WEBUI_TAB_STRIP_TAB_COUNTER_CXMENU_CLOSE_TAB),
      ui::ImageModel::FromVectorIcon(vector_icons::kCloseIcon,
                                     ui::kColorMenuIcon, gfx::kFaviconSize));
  menu_model_->AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);
  menu_model_->AddItemWithIcon(
      WEBUI_TAB_COUNTER_CXMENU_NEW_TAB,
      l10n_util::GetStringUTF16(IDS_WEBUI_TAB_STRIP_TAB_COUNTER_CXMENU_NEW_TAB),
      ui::ImageModel::FromVectorIcon(kAddIcon, ui::kColorMenuIcon,
                                     gfx::kFaviconSize));
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::HAS_MNEMONICS |
                             views::MenuRunner::CONTEXT_MENU |
                             views::MenuRunner::FIXED_ANCHOR);

  tab_strip_model_->AddObserver(this);
  const int tab_count = tab_strip_model_->count();
  UpdateTooltip(tab_count);
  appearing_label_->SetText(GetTabCounterLabelText(tab_count));
}

void WebUITabCounterButton::AddedToWidget() {
  interaction_tracker_ = std::make_unique<InteractionTracker>(GetWidget());
  link_opened_from_gesture_subscription_ =
      browser_view_->AddOnLinkOpeningFromGestureCallback(
          base::BindRepeating(&WebUITabCounterButton::MaybeStartFlyingLink,
                              base::Unretained(this)));
}

void WebUITabCounterButton::AfterPropertyChange(const void* key,
                                                int64_t old_value) {
  View::AfterPropertyChange(key, old_value);
  if (key != user_education::kHasInProductHelpPromoKey)
    return;
  UpdateColors();
}

void WebUITabCounterButton::AddLayerToRegion(ui::Layer* new_layer,
                                             views::LayerRegion region) {
  ink_drop_container_->AddLayerToRegion(new_layer, region);
}

void WebUITabCounterButton::RemoveLayerFromRegions(ui::Layer* old_layer) {
  ink_drop_container_->RemoveLayerFromRegions(old_layer);
}

void WebUITabCounterButton::OnThemeChanged() {
  views::Button::OnThemeChanged();
  UpdateColors();
}

void WebUITabCounterButton::Layout(PassKey) {
  const gfx::Rect view_bounds = GetLocalBounds();

  ink_drop_container_->SetBoundsRect(view_bounds);

  // Position views from the outside in (because it's easier).
  // Start with the throbber.
  const int throbber_height = GetLayoutConstant(LOCATION_BAR_HEIGHT);
  gfx::Rect throbber_rect = view_bounds;
  throbber_rect.ClampToCenteredSize(
      gfx::Size(throbber_height, throbber_height));
  throbber_->SetBoundsRect(throbber_rect);

  // Next is the rounded rect border around the counter.
  constexpr gfx::Size kDesiredBorderSize(22, 22);
  gfx::Rect border_bounds = view_bounds;
  border_bounds.ClampToCenteredSize(kDesiredBorderSize);
  border_view_->SetBoundsRect(border_bounds);

  // Finally is the numbers themselves, which nest inside the label view.
  appearing_label_->SetBoundsRect(gfx::Rect(kDesiredBorderSize));
  disappearing_label_->SetBoundsRect(
      gfx::Rect(gfx::Point(0, -kOffscreenLabelDistance), kDesiredBorderSize));

  // Adjust label positions for animation.
  animator_->LayoutIfAnimating();
}

void WebUITabCounterButton::MaybeStartFlyingLink(
    WindowOpenDisposition disposition) {
  if (disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB &&
      interaction_tracker_ &&
      interaction_tracker_->last_interaction_location().has_value())
    animator_->StartFlyingLinkFrom(
        interaction_tracker_->last_interaction_location().value());
}

void WebUITabCounterButton::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  const int num_tabs = tab_strip_model->count();
  UpdateTooltip(num_tabs);
  animator_->Animate(num_tabs,
                     ShouldChangeStartThrobber(tab_strip_model, change));
}

void WebUITabCounterButton::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  menu_runner_->RunMenuAt(GetWidget(), nullptr,
                          border_view_->GetBoundsInScreen(),
                          views::MenuAnchorPosition::kTopRight, source_type);
}

void WebUITabCounterButton::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case WEBUI_TAB_COUNTER_CXMENU_CLOSE_TAB: {
      tab_strip_model_->CloseWebContentsAt(
          tab_strip_model_->active_index(),
          TabCloseTypes::CLOSE_USER_GESTURE |
              TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
      break;
    }
    case WEBUI_TAB_COUNTER_CXMENU_NEW_TAB:
      tab_strip_model_->delegate()->AddTabAt(GURL(), -1, true);
      break;
    default:
      NOTREACHED();
  }
}

BEGIN_METADATA(WebUITabCounterButton)
END_METADATA

}  // namespace

std::unique_ptr<views::View> CreateWebUITabCounterButton(
    views::Button::PressedCallback pressed_callback,
    BrowserView* browser_view) {
  auto tab_counter = std::make_unique<WebUITabCounterButton>(
      std::move(pressed_callback), browser_view);

  tab_counter->Init();

  return tab_counter;
}
