// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/find_bar_host.h"

#include <algorithm>

#include "base/check_is_test.h"
#include "base/i18n/rtl.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/find_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/find_in_page/find_types.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/border.h"
#include "ui/views/focus/external_focus_tracker.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if defined(IS_AURA)
#include "ui/aura/window.h"
#include "ui/views/view_constants_aura.h"
#endif

using input::NativeWebKeyboardEvent;

namespace {

class FindBarHostHelper
    : public content::WebContentsUserData<FindBarHostHelper> {
 public:
  static FindBarHostHelper* CreateOrGetFromWebContents(
      content::WebContents* web_contents) {
    CreateForWebContents(web_contents);
    return FromWebContents(web_contents);
  }

  void SetExternalFocusTracker(
      std::unique_ptr<views::ExternalFocusTracker> external_focus_tracker) {
    external_focus_tracker_ = std::move(external_focus_tracker);
  }

  std::unique_ptr<views::ExternalFocusTracker> TakeExternalFocusTracker() {
    return std::move(external_focus_tracker_);
  }

  views::ExternalFocusTracker* focus_tracker() {
    return external_focus_tracker_.get();
  }

 private:
  friend class content::WebContentsUserData<FindBarHostHelper>;

  explicit FindBarHostHelper(content::WebContents* web_contents)
      : content::WebContentsUserData<FindBarHostHelper>(*web_contents) {}

  std::unique_ptr<views::ExternalFocusTracker> external_focus_tracker_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(FindBarHostHelper);

gfx::Rect GetLocationForFindBarView(gfx::Rect view_location,
                                    const gfx::Rect& dialog_bounds,
                                    const gfx::Rect& avoid_overlapping_rect) {
  // Clamp to the `dialog_bounds`.
  view_location.set_width(
      std::min(view_location.width(), dialog_bounds.width()));
  if (base::i18n::IsRTL()) {
    int boundary = dialog_bounds.width() - view_location.width();
    view_location.set_x(std::min(view_location.x(), boundary));
  } else {
    view_location.set_x(std::max(view_location.x(), dialog_bounds.x()));
  }

  gfx::Rect new_pos = view_location;

  // The minimum space between the FindInPage window and the search result.
  constexpr int kMinFindWndDistanceFromSelection = 5;

  // If the selection rectangle intersects the current position on screen then
  // we try to move our dialog to the left (right for RTL) of the selection
  // rectangle.
  if (!avoid_overlapping_rect.IsEmpty() &&
      avoid_overlapping_rect.Intersects(new_pos)) {
    if (base::i18n::IsRTL()) {
      new_pos.set_x(avoid_overlapping_rect.x() +
                    avoid_overlapping_rect.width() +
                    (2 * kMinFindWndDistanceFromSelection));

      // If we moved it off-screen to the right, we won't move it at all.
      if (new_pos.x() + new_pos.width() > dialog_bounds.width())
        new_pos = view_location;  // Reset.
    } else {
      new_pos.set_x(avoid_overlapping_rect.x() - new_pos.width() -
                    kMinFindWndDistanceFromSelection);

      // If we moved it off-screen to the left, we won't move it at all.
      if (new_pos.x() < 0)
        new_pos = view_location;  // Reset.
    }
  }

  return new_pos;
}

// During testing we can disable animations by setting this flag to true,
// so that opening and closing the dropdown bar is shown instantly, instead of
// having to poll it while it animates to open/closed status.
// TODO(https://crbug.com/40183900): Make this private and push disabling for
// testing into here instead of `find_bar_host_unittest_util`.
static bool kDisableAnimationsForTesting = false;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// FindBarHost, public:

FindBarHost::FindBarHost(BrowserView* browser_view)
    : AnimationDelegateViews(browser_view), browser_view_(browser_view) {
  auto find_bar_view = std::make_unique<FindBarView>(this);
  // The |clip_view| exists to paint to a layer so that it can clip descendent
  // Views which also paint to a Layer. See http://crbug.com/589497
  auto clip_view = std::make_unique<views::View>();
  clip_view->SetPaintToLayer();
  clip_view->layer()->SetFillsBoundsOpaquely(false);
  clip_view->layer()->SetMasksToBounds(true);
  view_ = clip_view->AddChildView(std::move(find_bar_view));

  // Initialize the host.
  host_ = std::make_unique<ThemeCopyingWidget>(browser_view_->GetWidget());
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_CONTROL);
  params.delegate = this;
  params.name = "FindBarHost";
  params.parent = browser_view_->GetWidgetForAnchoring()->GetNativeView();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
#if BUILDFLAG(IS_MAC)
  params.activatable = views::Widget::InitParams::Activatable::kYes;
#endif
  host_->Init(std::move(params));
  host_->SetContentsView(std::move(clip_view));
#if defined(IS_AURA)
  host_->GetNativeView()->SetProperty(views::kHostViewKey,
                                      browser_view->find_bar_host_view());
#endif

  // Start listening to focus changes, so we can register and unregister our
  // own handler for Escape.
  focus_manager_ = host_->GetFocusManager();
  focus_manager_->AddFocusChangeListener(this);

  animation_ = std::make_unique<gfx::SlideAnimation>(this);
  if (!gfx::Animation::ShouldRenderRichAnimation()) {
    animation_->SetSlideDuration(base::TimeDelta());
  }

  // Update the widget and |view_| bounds to the hidden state.
  AnimationProgressed(animation_.get());
  SetAccessibleWindowRole(ax::mojom::Role::kDialog);
}

FindBarHost::~FindBarHost() {
  focus_manager_->RemoveFocusChangeListener(this);
  focus_tracker_.reset();
}

bool FindBarHost::MaybeForwardKeyEventToWebpage(
    const ui::KeyEvent& key_event) {
  switch (key_event.key_code()) {
    case ui::VKEY_DOWN:
    case ui::VKEY_UP:
    case ui::VKEY_PRIOR:
    case ui::VKEY_NEXT:
      break;
    case ui::VKEY_HOME:
    case ui::VKEY_END:
      if (key_event.IsControlDown())
        break;
      [[fallthrough]];
    default:
      return false;
  }

  content::WebContents* contents = find_bar_controller_->web_contents();
  if (!contents)
    return false;

  // Make sure we don't have a text field element interfering with keyboard
  // input. Otherwise Up and Down arrow key strokes get eaten. "Nom Nom Nom".
  contents->ClearFocusedElement();
  NativeWebKeyboardEvent event(key_event);
  contents->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardKeyboardEventWithLatencyInfo(event, *key_event.latency());
  return true;
}

bool FindBarHost::IsVisible() const {
  return is_visible_;
}

#if BUILDFLAG(IS_MAC)
views::Widget* FindBarHost::GetHostWidget() {
  return host_.get();
}
#endif

FindBarController* FindBarHost::GetFindBarController() const {
  return find_bar_controller_;
}

void FindBarHost::SetFindBarController(FindBarController* find_bar_controller) {
  find_bar_controller_ = find_bar_controller;
}

void FindBarHost::Show(bool animate) {
  RestoreOrCreateFocusTracker();
  DCHECK(host_);

  SetDialogPosition(GetDialogPosition(gfx::Rect()));

  // If we're in the middle of a close animation, stop it and skip to the end.
  // This ensures that the state is consistent and prepared to show the drop-
  // down bar.
  if (animation_->IsClosing()) {
    animation_->End();
  }

  host_->Show();

  bool was_visible = is_visible_;
  is_visible_ = true;
  if (!animate || kDisableAnimationsForTesting) {
    animation_->Reset(1);
    AnimationProgressed(animation_.get());
  } else if (!was_visible) {
    // Don't re-start the animation.
    animation_->Reset();
    animation_->Show();
  }

  if (!was_visible) {
    OnVisibilityChanged();
  }
}

void FindBarHost::Hide(bool animate) {
  // Restore/Save is non-symmetric as hiding the FindBarHost could change
  // the focus state of the external view. Saving the focus tracker before the
  // hide preserves the appropriate view in the event the FindBarHost visibility
  // is restored as part of a tab change.
  SaveFocusTracker();
  if (!is_visible_) {
    return;
  }

  if (animate && !kDisableAnimationsForTesting && !animation_->IsClosing()) {
    animation_->Hide();
  } else {
    if (animation_->IsClosing()) {
      // If we're in the middle of a close animation, skip immediately to the
      // end of the animation.
      animation_->End();
    } else {
      // Otherwise we need to set both the animation state to ended and the
      // DropdownBarHost state to ended/hidden, otherwise the next time we try
      // to show the bar, it might refuse to do so. Note that we call
      // AnimationEnded ourselves as Reset does not call it if we are not
      // animating here.
      animation_->Reset();
      AnimationEnded(animation_.get());
    }
  }
}

void FindBarHost::SetFocusAndSelection() {
  view_->FocusAndSelectAll();
}

void FindBarHost::ClearResults(
    const find_in_page::FindNotificationDetails& results) {
  view_->UpdateForResult(results, std::u16string());
}

void FindBarHost::StopAnimation() {
  animation_->End();
}

void FindBarHost::MoveWindowIfNecessary() {
  MoveWindowIfNecessaryWithRect(gfx::Rect());
}

void FindBarHost::SetFindTextAndSelectedRange(
    const std::u16string& find_text,
    const gfx::Range& selected_range) {
  view_->SetFindTextAndSelectedRange(find_text, selected_range);
}

std::u16string FindBarHost::GetFindText() const {
  return view_->GetFindText();
}

gfx::Range FindBarHost::GetSelectedRange() const {
  return view_->GetSelectedRange();
}

void FindBarHost::UpdateUIForFindResult(
    const find_in_page::FindNotificationDetails& result,
    const std::u16string& find_text) {
  if (!find_text.empty())
    view_->UpdateForResult(result, find_text);
  else
    view_->ClearMatchCount();

  // We now need to check if the window is obscuring the search results.
  MoveWindowIfNecessaryWithRect(result.selection_rect());

  // Once we find a match we no longer want to keep track of what had
  // focus. EndFindSession will then set the focus to the page content.
  if (result.number_of_matches() > 0)
    focus_tracker_.reset();
}

void FindBarHost::AudibleAlert() {
  ++audible_alerts_;
#if BUILDFLAG(IS_WIN)
  MessageBeep(MB_OK);
#endif
}

bool FindBarHost::IsFindBarVisible() const {
  return is_visible_;
}

void FindBarHost::RestoreSavedFocus() {
  std::unique_ptr<views::ExternalFocusTracker> focus_tracker_from_web_contents;
  views::ExternalFocusTracker* tracker = focus_tracker_.get();
  if (!tracker) {
    auto* web_contents = find_bar_controller_->web_contents();
    if (web_contents) {
      auto* helper = FindBarHostHelper::FromWebContents(web_contents);
      if (helper) {
        focus_tracker_from_web_contents = helper->TakeExternalFocusTracker();
        tracker = focus_tracker_from_web_contents.get();
      }
    }
  }

  if (tracker) {
    tracker->FocusLastFocusedExternalView();
    focus_tracker_.reset();
  } else {
    // TODO(brettw): Focus() should be on WebContentsView.
    find_bar_controller_->web_contents()->Focus();
  }
}

bool FindBarHost::HasGlobalFindPasteboard() const {
#if BUILDFLAG(IS_MAC)
  return true;
#else
  return false;
#endif
}

void FindBarHost::UpdateFindBarForChangedWebContents() {
}

const FindBarTesting* FindBarHost::GetFindBarTesting() const {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// FindBarWin, ui::AcceleratorTarget implementation:

bool FindBarHost::AcceleratorPressed(const ui::Accelerator& accelerator) {
  ui::KeyboardCode key = accelerator.key_code();
  if (key == ui::VKEY_RETURN && accelerator.IsCtrlDown()) {
    // Ctrl+Enter closes the Find session and navigates any link that is active.
    find_bar_controller_->EndFindSession(
        find_in_page::SelectionAction::kActivate,
        find_in_page::ResultAction::kClear);
    return true;
  }

  CHECK_EQ(key, ui::VKEY_ESCAPE);
  // This will end the Find session and hide the window, causing it to loose
  // focus and in the process unregister us as the handler for the Escape
  // accelerator through the OnWillChangeFocus event.
  find_bar_controller_->EndFindSession(find_in_page::SelectionAction::kKeep,
                                       find_in_page::ResultAction::kKeep);
  return true;
}

bool FindBarHost::CanHandleAccelerators() const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// FindBarTesting implementation:

bool FindBarHost::GetFindBarWindowInfo(gfx::Point* position,
                                       bool* fully_visible) const {
  if (!find_bar_controller_) {
    if (position)
      *position = gfx::Point();
    if (fully_visible)
      *fully_visible = false;
    return false;
  }

  gfx::Rect window_rect = host_->GetWindowBoundsInScreen();
  if (position)
    *position = window_rect.origin();
  if (fully_visible)
    *fully_visible = is_visible_ && !animation_->is_animating();
  return true;
}

std::u16string FindBarHost::GetFindSelectedText() const {
  return view_->GetFindSelectedText();
}

std::u16string FindBarHost::GetMatchCountText() const {
  return view_->GetMatchCountText();
}

int FindBarHost::GetContentsWidth() const {
  return view_->GetContentsBounds().width();
}

size_t FindBarHost::GetAudibleAlertCount() const {
  return audible_alerts_;
}

std::u16string FindBarHost::GetAccessibleWindowTitle() const {
  // This can be called in tests by AccessibilityChecker before the controller
  // is registered with this object. So to handle that case, we need to bail out
  // if there is no controller.
  const FindBarController* const controller = GetFindBarController();
  if (!controller)
    return std::u16string();
  return l10n_util::GetStringFUTF16(
      IDS_FIND_IN_PAGE_ACCESSIBLE_TITLE,
      browser_view_->browser()->GetWindowTitleForCurrentTab(false));
}

FindBarView* FindBarHost::GetFindBarViewForTesting() {
  CHECK_IS_TEST();
  return view_;
}

void FindBarHost::SetEnableAnimationsForTesting(bool enable_animations) {
  CHECK_IS_TEST();
  kDisableAnimationsForTesting = !enable_animations;
}
////////////////////////////////////////////////////////////////////////////////
// private:

void FindBarHost::GetWidgetPositionNative(gfx::Rect* avoid_overlapping_rect) {
  gfx::Rect frame_rect = host_->GetTopLevelWidget()->GetWindowBoundsInScreen();
  gfx::Rect webcontents_rect =
      find_bar_controller_->web_contents()->GetViewBounds();
  avoid_overlapping_rect->Offset(0, webcontents_rect.y() - frame_rect.y());
}

void FindBarHost::MoveWindowIfNecessaryWithRect(
    const gfx::Rect& selection_rect) {
  // We only move the window if one is active for the current WebContents. If we
  // don't check this, then SetDialogPosition below will end up making the Find
  // Bar visible.
  content::WebContents* web_contents = find_bar_controller_->web_contents();
  if (!web_contents)
    return;

  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents);
  if (!find_tab_helper || !find_tab_helper->find_ui_active())
    return;

  gfx::Rect new_pos = GetDialogPosition(selection_rect);
  SetDialogPosition(new_pos);

  // May need to redraw our frame to accommodate bookmark bar styles.
  view_->DeprecatedLayoutImmediately();  // Bounds may have changed.
  view_->SchedulePaint();
}

void FindBarHost::SaveFocusTracker() {
  auto* web_contents = find_bar_controller_->web_contents();
  if (!web_contents)
    return;

  if (focus_tracker_) {
    focus_tracker_->SetFocusManager(nullptr);
    FindBarHostHelper::CreateOrGetFromWebContents(web_contents)
        ->SetExternalFocusTracker(std::move(focus_tracker_));
  }
}

void FindBarHost::RestoreOrCreateFocusTracker() {
  auto* web_contents = find_bar_controller_->web_contents();
  if (!web_contents)
    return;

  std::unique_ptr<views::ExternalFocusTracker> focus_tracker =
      FindBarHostHelper::CreateOrGetFromWebContents(web_contents)
          ->TakeExternalFocusTracker();
  if (focus_tracker) {
    focus_tracker_ = std::move(focus_tracker);
    focus_tracker_->SetFocusManager(host_->GetFocusManager());
  } else {
    focus_tracker_ =
        std::make_unique<views::ExternalFocusTracker>(view_, focus_manager_);
  }
}

void FindBarHost::OnVisibilityChanged() {
  // Tell the immersive mode controller about the find bar's bounds. The
  // immersive mode controller uses the bounds to keep the top-of-window views
  // revealed when the mouse is hovered over the find bar.
  gfx::Rect visible_bounds;
  if (is_visible_) {
    visible_bounds = host_->GetWindowBoundsInScreen();
  }
  browser_view_->immersive_mode_controller()->OnFindBarVisibleBoundsChanged(
      visible_bounds);

  browser_view_->browser()->OnFindBarVisibilityChanged();
}

void FindBarHost::RegisterAccelerators() {
  DCHECK(!esc_accel_target_registered_);
  ui::Accelerator escape(ui::VKEY_ESCAPE, ui::EF_NONE);
  focus_manager_->RegisterAccelerator(
      escape, ui::AcceleratorManager::kNormalPriority, this);
  esc_accel_target_registered_ = true;
  // Register for Ctrl+Return.
  ui::Accelerator ctrl_ret(ui::VKEY_RETURN, ui::EF_CONTROL_DOWN);
  focus_manager_->RegisterAccelerator(
      ctrl_ret, ui::AcceleratorManager::kNormalPriority, this);
}

void FindBarHost::UnregisterAccelerators() {
  // Unregister Ctrl+Return.
  ui::Accelerator ctrl_ret(ui::VKEY_RETURN, ui::EF_CONTROL_DOWN);
  focus_manager_->UnregisterAccelerator(ctrl_ret, this);

  DCHECK(esc_accel_target_registered_);
  ui::Accelerator escape(ui::VKEY_ESCAPE, ui::EF_NONE);
  focus_manager_->UnregisterAccelerator(escape, this);
  esc_accel_target_registered_ = false;
}

gfx::Rect FindBarHost::GetDialogPosition(gfx::Rect avoid_overlapping_rect) {
  // Find the area we have to work with (after accounting for scrollbars, etc).
  // The BrowserView does Layout for the components that we care about
  // positioning relative to, so we ask it to tell us where we should go.
  gfx::Rect find_bar_bounds = browser_view_->GetFindBarBoundingBox();
  if (find_bar_bounds.IsEmpty()) {
    return gfx::Rect();
  }

  // Ask the view how large an area it needs to draw on.
  gfx::Size prefsize = view_->GetPreferredSize();

  // Don't show the find bar if |widget_bounds| is not tall enough to fit.
  gfx::Insets insets = view_->GetInsets();
  if (find_bar_bounds.height() < prefsize.height() - insets.height()) {
    return gfx::Rect();
  }

  // Place the view in the top right corner of the widget boundaries (top left
  // for RTL languages). Adjust for the view insets to ensure the border lines
  // up with the location bar.
  int x = find_bar_bounds.x() - insets.left();
  if (!base::i18n::IsRTL()) {
    x += find_bar_bounds.width() - prefsize.width() + insets.width();
  }
  int y = find_bar_bounds.y() - insets.top();
  const gfx::Rect view_location(x, y, prefsize.width(), prefsize.height());

  // When we get Find results back, we specify a selection rect, which we
  // should strive to avoid overlapping. But first, we need to offset the
  // selection rect (if one was provided).
  if (!avoid_overlapping_rect.IsEmpty()) {
    // For comparison (with the Intersects function below) we need to account
    // for the fact that we draw the Find widget relative to the Chrome frame,
    // whereas the selection rect is relative to the page.
    GetWidgetPositionNative(&avoid_overlapping_rect);
  }

  gfx::Rect widget_bounds = browser_view_->bounds();

  return GetLocationForFindBarView(view_location, widget_bounds,
                                   avoid_overlapping_rect);
}

void FindBarHost::SetDialogPosition(const gfx::Rect& new_pos) {
  view_->SetSize(new_pos.size());

  if (new_pos.IsEmpty()) {
    return;
  }

  host_->SetBounds(new_pos);

  // Tell the immersive mode controller about the find bar's new bounds. The
  // immersive mode controller uses the bounds to keep the top-of-window views
  // revealed when the mouse is hovered over the find bar.
  browser_view_->immersive_mode_controller()->OnFindBarVisibleBoundsChanged(
      host_->GetWindowBoundsInScreen());

  browser_view_->browser()->OnFindBarVisibilityChanged();
}

void FindBarHost::OnWillChangeFocus(views::View* focused_before,
                                    views::View* focused_now) {
  // First we need to determine if one or both of the views passed in are child
  // views of our view.
  bool our_view_before = focused_before && view_->Contains(focused_before);
  bool our_view_now = focused_now && view_->Contains(focused_now);

  // When both our_view_before and our_view_now are false, it means focus is
  // changing hands elsewhere in the application (and we shouldn't do anything).
  // Similarly, when both are true, focus is changing hands within the dropdown
  // widget (and again, we should not do anything). We therefore only need to
  // look at when we gain initial focus and when we loose it.
  if (!our_view_before && our_view_now) {
    // We are gaining focus from outside the dropdown widget so we must register
    // a handler for Escape.
    RegisterAccelerators();
  } else if (our_view_before && !our_view_now) {
    // We are losing focus to something outside our widget so we restore the
    // original handler for Escape.
    UnregisterAccelerators();
  }
}

void FindBarHost::OnDidChangeFocus(views::View* focused_before,
                                   views::View* focused_now) {}

void FindBarHost::AnimationProgressed(const gfx::Animation* animation) {
  // First, we calculate how many pixels to slide the widget.
  gfx::Size pref_size = view_->GetPreferredSize();
  int view_offset = static_cast<int>((animation_->GetCurrentValue() - 1.0) *
                                     pref_size.height());

  // This call makes sure |view_| appears in the right location, the size and
  // shape is correct and that it slides in the right direction.
  view_->SetPosition(gfx::Point(0, view_offset));
}

void FindBarHost::AnimationEnded(const gfx::Animation* animation) {
  // Ensure the position gets a final update.  This is important when ending the
  // animation early (e.g. closing a tab with an open find bar), since otherwise
  // the position will be out of date at the start of the next animation.
  AnimationProgressed(animation);

  if (!animation_->IsShowing()) {
    // Animation has finished closing.
    DCHECK(host_);
    host_->Hide();
    is_visible_ = false;
    OnVisibilityChanged();
  }
}
