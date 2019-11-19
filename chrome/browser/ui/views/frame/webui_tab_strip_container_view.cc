// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"

#include <utility>

#include "base/i18n/message_formatter.h"
#include "base/i18n/number_formatting.h"
#include "base/logging.h"
#include "base/scoped_observer.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_layout.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

namespace {

bool EventTypeCanCloseTabStrip(const ui::EventType& type) {
  switch (type) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_TOUCH_PRESSED:
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_DOUBLE_TAP:
      return true;
    default:
      return false;
  }
}

}  // namespace

// When enabled, closes the container upon any event in the window not
// destined for the container and cancels the event. If an event is
// destined for the container, it passes it through.
class WebUITabStripContainerView::AutoCloser : public ui::EventHandler {
 public:
  using EventPassthroughPredicate =
      base::RepeatingCallback<bool(const ui::Event& event)>;

  AutoCloser(EventPassthroughPredicate event_passthrough_predicate,
             base::RepeatingClosure close_container_callback)
      : event_passthrough_predicate_(std::move(event_passthrough_predicate)),
        close_container_callback_(std::move(close_container_callback)) {}

  ~AutoCloser() override {}

  // Sets whether to inspect events. If not enabled, all events are
  // ignored and passed through as usual.
  void set_enabled(bool enabled) { enabled_ = enabled; }

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    if (!enabled_)
      return;
    if (event_passthrough_predicate_.Run(*event))
      return;

    event->StopPropagation();
    close_container_callback_.Run();
  }

 private:
  EventPassthroughPredicate event_passthrough_predicate_;
  base::RepeatingClosure close_container_callback_;
  bool enabled_ = false;
};

class TabCounterModelObserver : public TabStripModelObserver {
 public:
  explicit TabCounterModelObserver(views::LabelButton* tab_counter)
      : tab_counter_(tab_counter) {}
  ~TabCounterModelObserver() override = default;

  void UpdateCounter(TabStripModel* model) {
    const int num_tabs = model->count();

    tab_counter_->SetTooltipText(
        base::i18n::MessageFormatter::FormatWithNumberedArgs(
            l10n_util::GetStringUTF16(IDS_TOOLTIP_WEBUI_TAB_STRIP_TAB_COUNTER),
            num_tabs));
    // TODO(999557): Have a 99+-style fallback to limit the max text width.
    tab_counter_->SetText(base::FormatNumber(num_tabs));
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    UpdateCounter(tab_strip_model);
  }

 private:
  views::LabelButton* tab_counter_;
};

WebUITabStripContainerView::WebUITabStripContainerView(
    Browser* browser,
    views::View* tab_contents_container)
    : browser_(browser),
      web_view_(
          AddChildView(std::make_unique<views::WebView>(browser->profile()))),
      tab_contents_container_(tab_contents_container),
      auto_closer_(std::make_unique<AutoCloser>(
          base::Bind(&WebUITabStripContainerView::EventShouldPropagate,
                     base::Unretained(this)),
          base::Bind(&WebUITabStripContainerView::CloseContainer,
                     base::Unretained(this)))) {
  DCHECK(UseTouchableTabStrip());
  animation_.SetTweenType(gfx::Tween::Type::FAST_OUT_SLOW_IN);

  SetVisible(false);
  // TODO(crbug.com/1010589) WebContents are initially assumed to be visible by
  // default unless explicitly hidden. The WebContents need to be set to hidden
  // so that the visibility state of the document in JavaScript is correctly
  // initially set to 'hidden', and the 'visibilitychange' events correctly get
  // fired.
  web_view_->GetWebContents()->WasHidden();

  web_view_->set_allow_accelerators(true);

  SetLayoutManager(std::make_unique<views::FillLayout>());
  web_view_->LoadInitialURL(GURL(chrome::kChromeUITabStripURL));
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_view_->web_contents());
  task_manager::WebContentsTags::CreateForTabContents(
      web_view_->web_contents());

  DCHECK(tab_contents_container);
  view_observer_.Add(tab_contents_container_);
  desired_height_ = TabStripUILayout::CalculateForWebViewportSize(
                        tab_contents_container_->size())
                        .CalculateContainerHeight();

  TabStripUI* const tab_strip_ui = static_cast<TabStripUI*>(
      web_view_->GetWebContents()->GetWebUI()->GetController());
  tab_strip_ui->Initialize(browser_, this);

  // Our observed Widget's NativeView may be destroyed before us. We
  // have no reasonable way of un-registering our pre-target handler
  // from the NativeView while the Widget is destroying. This disables
  // EventHandler's check that it has been removed from all
  // EventTargets.
  auto_closer_->DisableCheckTargets();
}

WebUITabStripContainerView::~WebUITabStripContainerView() {
  // The NewTabButton and TabCounter button both use |this| as a listener. We
  // need to make sure we outlive them.
  delete new_tab_button_;
  delete tab_counter_;
}

bool WebUITabStripContainerView::UseTouchableTabStrip() {
  return base::FeatureList::IsEnabled(features::kWebUITabStrip) &&
         ui::MaterialDesignController::touch_ui();
}

views::NativeViewHost* WebUITabStripContainerView::GetNativeViewHost() {
  return web_view_->holder();
}

std::unique_ptr<ToolbarButton>
WebUITabStripContainerView::CreateNewTabButton() {
  DCHECK_EQ(nullptr, new_tab_button_);
  auto new_tab_button = std::make_unique<ToolbarButton>(this);
  new_tab_button->SetID(VIEW_ID_WEBUI_TAB_STRIP_NEW_TAB_BUTTON);
  new_tab_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_NEW_TAB));

  new_tab_button_ = new_tab_button.get();
  view_observer_.Add(new_tab_button_);

  return new_tab_button;
}

std::unique_ptr<views::View> WebUITabStripContainerView::CreateTabCounter() {
  DCHECK_EQ(nullptr, tab_counter_);
  // TODO(999557): Create a custom text style to get the correct size/weight.
  // TODO(999557): Figure out how to get the right font.
  auto tab_counter = std::make_unique<views::LabelButton>(
      this, base::string16(), views::style::CONTEXT_BUTTON_MD);
  tab_counter->SetID(VIEW_ID_WEBUI_TAB_STRIP_TAB_COUNTER);
  tab_counter->SetProperty(views::kFlexBehaviorKey,
                           views::FlexSpecification::ForSizeRule(
                               views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred)
                               .WithOrder(1));

  // TODO(999557): also update this in response to touch mode changes.
  const int button_height = GetLayoutConstant(TOOLBAR_BUTTON_HEIGHT);
  tab_counter->SetMinSize(gfx::Size(button_height, button_height));
  tab_counter->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);

  // TODO(999557): Install an inkdrop.

  // TODO(999557): Add a roundrect border, like below but more like spec.
  // tab_counter->SetBorder(views::CreateRoundedRectBorder(
  //     2,
  //     views::LayoutProvider::Get()->GetCornerRadiusMetric(
  //         views::EMPHASIS_MEDIUM),
  //     gfx::kGoogleGrey300));

  tab_counter_model_observer_ =
      std::make_unique<TabCounterModelObserver>(tab_counter.get());
  browser_->tab_strip_model()->AddObserver(tab_counter_model_observer_.get());
  tab_counter_model_observer_->UpdateCounter(browser_->tab_strip_model());

  tab_counter_ = tab_counter.get();
  view_observer_.Add(tab_counter_);

  return tab_counter;
}

void WebUITabStripContainerView::UpdateButtons() {
  const SkColor normal_color =
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
  if (new_tab_button_) {
    new_tab_button_->SetImage(views::Button::STATE_NORMAL,
                              gfx::CreateVectorIcon(kAddIcon, normal_color));
  }
  if (tab_counter_)
    tab_counter_->SetEnabledTextColors(normal_color);
}

void WebUITabStripContainerView::CloseContainer() {
  SetContainerTargetVisibility(false);
}

void WebUITabStripContainerView::SetContainerTargetVisibility(
    bool target_visible) {
  if (target_visible) {
    SetVisible(true);
    animation_.SetSlideDuration(base::TimeDelta::FromMilliseconds(250));
    animation_.Show();
    web_view_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  } else {
    animation_.SetSlideDuration(base::TimeDelta::FromMilliseconds(200));
    animation_.Hide();
    web_view_->SetFocusBehavior(FocusBehavior::NEVER);
  }
  auto_closer_->set_enabled(target_visible);
}

bool WebUITabStripContainerView::EventShouldPropagate(const ui::Event& event) {
  if (!event.IsLocatedEvent())
    return true;
  const ui::LocatedEvent* located_event = event.AsLocatedEvent();

  if (!EventTypeCanCloseTabStrip(located_event->type()))
    return true;

  // If the event is in the container or control buttons, let it be handled.
  for (views::View* view : {static_cast<views::View*>(this),
                            static_cast<views::View*>(new_tab_button_),
                            static_cast<views::View*>(tab_counter_)}) {
    if (!view)
      continue;

    const gfx::Rect bounds_in_window =
        view->ConvertRectToWidget(view->GetLocalBounds());
    if (bounds_in_window.Contains(located_event->root_location()))
      return true;
  }

  // Otherwise, cancel the event and close the container.
  return false;
}

void WebUITabStripContainerView::AnimationEnded(
    const gfx::Animation* animation) {
  DCHECK_EQ(&animation_, animation);
  if (animation_.GetCurrentValue() == 0.0)
    SetVisible(false);
}

void WebUITabStripContainerView::AnimationProgressed(
    const gfx::Animation* animation) {
  PreferredSizeChanged();
}

void WebUITabStripContainerView::ShowContextMenuAtPoint(
    gfx::Point point,
    std::unique_ptr<ui::MenuModel> menu_model) {
  ConvertPointToScreen(this, &point);
  context_menu_model_ = std::move(menu_model);
  context_menu_runner_ = std::make_unique<views::MenuRunner>(
      context_menu_model_.get(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  context_menu_runner_->RunMenuAt(
      GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
      views::MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_MOUSE);
}

TabStripUILayout WebUITabStripContainerView::GetLayout() {
  return TabStripUILayout::CalculateForWebViewportSize(
      tab_contents_container_->size());
}

void WebUITabStripContainerView::AddedToWidget() {
  GetWidget()->GetNativeView()->AddPreTargetHandler(auto_closer_.get());
}

void WebUITabStripContainerView::RemovedFromWidget() {
  aura::Window* const native_view = GetWidget()->GetNativeView();
  if (native_view)
    native_view->RemovePreTargetHandler(auto_closer_.get());
}

int WebUITabStripContainerView::GetHeightForWidth(int w) const {
  return desired_height_ * animation_.GetCurrentValue();
}

void WebUITabStripContainerView::ButtonPressed(views::Button* sender,
                                               const ui::Event& event) {
  if (sender->GetID() == VIEW_ID_WEBUI_TAB_STRIP_TAB_COUNTER) {
    SetContainerTargetVisibility(!GetVisible());

    if (GetVisible() && sender->HasFocus()) {
      // Automatically move focus to the tab strip WebUI if the focus is
      // currently on the toggle button.
      SetPaneFocus(web_view_);
    }
  } else if (sender->GetID() == VIEW_ID_WEBUI_TAB_STRIP_NEW_TAB_BUTTON) {
    chrome::ExecuteCommand(browser_, IDC_NEW_TAB);
  } else {
    NOTREACHED();
  }
}

void WebUITabStripContainerView::OnViewBoundsChanged(View* observed_view) {
  if (observed_view != tab_contents_container_)
    return;

  desired_height_ =
      TabStripUILayout::CalculateForWebViewportSize(observed_view->size())
          .CalculateContainerHeight();
  // TODO(pbos): PreferredSizeChanged seems to cause infinite recursion with
  // BrowserView::ChildPreferredSizeChanged. InvalidateLayout here should be
  // replaceable with PreferredSizeChanged.
  InvalidateLayout();

  TabStripUI* const tab_strip_ui = static_cast<TabStripUI*>(
      web_view_->GetWebContents()->GetWebUI()->GetController());
  tab_strip_ui->LayoutChanged();
}

void WebUITabStripContainerView::OnViewIsDeleting(View* observed_view) {
  if (observed_view == new_tab_button_)
    new_tab_button_ = nullptr;
  else if (observed_view == tab_counter_)
    tab_counter_ = nullptr;
}
