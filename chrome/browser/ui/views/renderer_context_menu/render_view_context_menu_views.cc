// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/renderer_context_menu/render_view_context_menu_views.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/scoped_observation.h"
#include "base/task/current_thread.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/renderer_context_menu/views/toolkit_delegate_views.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/menu/menu_host.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

using content::WebContents;

class RenderViewContextMenuViews::SubmenuViewObserver
    : public views::ViewObserver,
      public views::WidgetObserver {
 public:
  SubmenuViewObserver(RenderViewContextMenuViews* parent,
                      views::SubmenuView* submenu_view)
      : parent_(parent), submenu_view_(submenu_view) {
    submenu_view_observation_.Observe(submenu_view);
    auto* widget = submenu_view_->host();
    if (widget)
      submenu_widget_observation_.Observe(widget);
  }

  SubmenuViewObserver(const SubmenuViewObserver&) = delete;
  SubmenuViewObserver& operator=(const SubmenuViewObserver&) = delete;

  ~SubmenuViewObserver() override = default;

  // ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override {
    // The submenu view is being deleted, make sure the parent no longer
    // observes it.
    DCHECK_EQ(submenu_view_, observed_view);
    parent_->OnSubmenuClosed();
  }

  void OnViewBoundsChanged(views::View* observed_view) override {
    DCHECK_EQ(submenu_view_, observed_view);
    parent_->OnSubmenuViewBoundsChanged(
        submenu_view_->host()->GetWindowBoundsInScreen());
  }

  void OnViewAddedToWidget(views::View* observed_view) override {
    DCHECK_EQ(submenu_view_, observed_view);
    auto* widget = submenu_view_->host();
    if (widget)
      submenu_widget_observation_.Observe(widget);
  }

  // WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds_in_screen) override {
    DCHECK_EQ(submenu_view_->host(), widget);
    parent_->OnSubmenuViewBoundsChanged(new_bounds_in_screen);
  }

  void OnWidgetClosing(views::Widget* widget) override {
    // The widget is being closed, make sure the parent bubble no longer
    // observes it.
    DCHECK_EQ(submenu_view_->host(), widget);
    parent_->OnSubmenuClosed();
  }

 private:
  RenderViewContextMenuViews* const parent_;
  views::SubmenuView* const submenu_view_;
  base::ScopedObservation<views::View, views::ViewObserver>
      submenu_view_observation_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      submenu_widget_observation_{this};
};

////////////////////////////////////////////////////////////////////////////////
// RenderViewContextMenuViews, public:

RenderViewContextMenuViews::RenderViewContextMenuViews(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params)
    : RenderViewContextMenu(render_frame_host, params),
      bidi_submenu_model_(this) {
  std::unique_ptr<ToolkitDelegate> delegate(new ToolkitDelegateViews);
  set_toolkit_delegate(std::move(delegate));
}

RenderViewContextMenuViews::~RenderViewContextMenuViews() {
}

// static
RenderViewContextMenuViews* RenderViewContextMenuViews::Create(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  return new RenderViewContextMenuViews(render_frame_host, params);
}

void RenderViewContextMenuViews::RunMenuAt(views::Widget* parent,
                                           const gfx::Point& point,
                                           ui::MenuSourceType type) {
  static_cast<ToolkitDelegateViews*>(toolkit_delegate())->
      RunMenuAt(parent, point, type);
}

////////////////////////////////////////////////////////////////////////////////
// RenderViewContextMenuViews, protected:

bool RenderViewContextMenuViews::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accel) const {
  // There are no formally defined accelerators we can query so we assume
  // that Ctrl+C, Ctrl+V, Ctrl+X, Ctrl-A, etc do what they normally do.
  switch (command_id) {
    case IDC_BACK:
      *accel = ui::Accelerator(ui::VKEY_LEFT, ui::EF_ALT_DOWN);
      return true;

    case IDC_CONTENT_CONTEXT_UNDO:
      *accel = ui::Accelerator(ui::VKEY_Z, ui::EF_CONTROL_DOWN);
      return true;

    case IDC_CONTENT_CONTEXT_REDO:
      // TODO(jcampan): should it be Ctrl-Y?
      *accel = ui::Accelerator(ui::VKEY_Z,
                               ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
      return true;

    case IDC_CONTENT_CONTEXT_CUT:
      *accel = ui::Accelerator(ui::VKEY_X, ui::EF_CONTROL_DOWN);
      return true;

    case IDC_CONTENT_CONTEXT_COPY:
      *accel = ui::Accelerator(ui::VKEY_C, ui::EF_CONTROL_DOWN);
      return true;

    case IDC_CONTENT_CONTEXT_INSPECTELEMENT:
      *accel = ui::Accelerator(ui::VKEY_I,
                               ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
      return true;

    case IDC_CONTENT_CONTEXT_PASTE:
      *accel = ui::Accelerator(ui::VKEY_V, ui::EF_CONTROL_DOWN);
      return true;

    case IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE:
      *accel = ui::Accelerator(ui::VKEY_V,
                               ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
      return true;

    case IDC_CONTENT_CONTEXT_SELECTALL:
      *accel = ui::Accelerator(ui::VKEY_A, ui::EF_CONTROL_DOWN);
      return true;

    case IDC_CONTENT_CONTEXT_ROTATECCW:
      *accel = ui::Accelerator(ui::VKEY_OEM_4, ui::EF_CONTROL_DOWN);
      return true;

    case IDC_CONTENT_CONTEXT_ROTATECW:
      *accel = ui::Accelerator(ui::VKEY_OEM_6, ui::EF_CONTROL_DOWN);
      return true;

    case IDC_FORWARD:
      *accel = ui::Accelerator(ui::VKEY_RIGHT, ui::EF_ALT_DOWN);
      return true;

    case IDC_PRINT:
      *accel = ui::Accelerator(ui::VKEY_P, ui::EF_CONTROL_DOWN);
      return true;

    case IDC_RELOAD:
      *accel = ui::Accelerator(ui::VKEY_R, ui::EF_CONTROL_DOWN);
      return true;

    case IDC_CONTENT_CONTEXT_SAVEAVAS:
    case IDC_SAVE_PAGE:
      *accel = ui::Accelerator(ui::VKEY_S, ui::EF_CONTROL_DOWN);
      return true;

    case IDC_CONTENT_CONTEXT_EXIT_FULLSCREEN: {
      // Esc only works in HTML5 (site-triggered) fullscreen.
      if (IsHTML5Fullscreen()) {
        // Per UX design feedback, do not show an accelerator when press and
        // hold is required to exit fullscreen.
        if (IsPressAndHoldEscRequiredToExitFullscreen())
          return false;

        *accel = ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE);
        return true;
      }

#if BUILDFLAG(IS_CHROMEOS_ASH)
      // Chromebooks typically do not have an F11 key, so do not show an
      // accelerator here.
      return false;
#else
      // User-triggered fullscreen. Show the shortcut for toggling fullscreen
      // (i.e., F11).
      ui::AcceleratorProvider* accelerator_provider =
          GetBrowserAcceleratorProvider();
      if (!accelerator_provider)
        return false;

      return accelerator_provider->GetAcceleratorForCommandId(IDC_FULLSCREEN,
                                                              accel);
#endif
    }

    case IDC_VIEW_SOURCE:
      *accel = ui::Accelerator(ui::VKEY_U, ui::EF_CONTROL_DOWN);
      return true;

    case IDC_CONTENT_CONTEXT_EMOJI:
#if defined(OS_WIN)
      *accel = ui::Accelerator(ui::VKEY_OEM_PERIOD, ui::EF_COMMAND_DOWN);
      return true;
#elif defined(OS_MAC)
      *accel = ui::Accelerator(ui::VKEY_SPACE,
                               ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN);
      return true;
#elif BUILDFLAG(IS_CHROMEOS_ASH)
      *accel = ui::Accelerator(ui::VKEY_SPACE,
                               ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);
      return true;
#else
      return false;
#endif

    case IDC_CONTENT_CLIPBOARD_HISTORY_MENU:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      *accel = ui::Accelerator(ui::VKEY_V, ui::EF_COMMAND_DOWN);
      return true;
#else
      NOTREACHED();
      return false;
#endif
    default:
      return false;
  }
}

void RenderViewContextMenuViews::ExecuteCommand(int command_id,
                                                int event_flags) {
  switch (command_id) {
    case IDC_WRITING_DIRECTION_DEFAULT:
      // WebKit's current behavior is for this menu item to always be disabled.
      NOTREACHED();
      break;

    case IDC_WRITING_DIRECTION_RTL:
    case IDC_WRITING_DIRECTION_LTR: {
      content::RenderViewHost* view_host = GetRenderViewHost();
      view_host->GetWidget()->UpdateTextDirection(
          (command_id == IDC_WRITING_DIRECTION_RTL)
              ? base::i18n::RIGHT_TO_LEFT
              : base::i18n::LEFT_TO_RIGHT);
      view_host->GetWidget()->NotifyTextDirection();
      RenderViewContextMenu::RecordUsedItem(command_id);
      break;
    }

    default:
      RenderViewContextMenu::ExecuteCommand(command_id, event_flags);
      break;
  }
}

bool RenderViewContextMenuViews::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case IDC_WRITING_DIRECTION_DEFAULT:
      return (params_.writing_direction_default &
              blink::ContextMenuData::kCheckableMenuItemChecked) != 0;
    case IDC_WRITING_DIRECTION_RTL:
      return (params_.writing_direction_right_to_left &
              blink::ContextMenuData::kCheckableMenuItemChecked) != 0;
    case IDC_WRITING_DIRECTION_LTR:
      return (params_.writing_direction_left_to_right &
              blink::ContextMenuData::kCheckableMenuItemChecked) != 0;

    default:
      return RenderViewContextMenu::IsCommandIdChecked(command_id);
  }
}

bool RenderViewContextMenuViews::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case IDC_WRITING_DIRECTION_MENU:
      return true;
    case IDC_WRITING_DIRECTION_DEFAULT:  // Provided to match OS defaults.
      return params_.writing_direction_default &
             blink::ContextMenuData::kCheckableMenuItemEnabled;
    case IDC_WRITING_DIRECTION_RTL:
      return params_.writing_direction_right_to_left &
             blink::ContextMenuData::kCheckableMenuItemEnabled;
    case IDC_WRITING_DIRECTION_LTR:
      return params_.writing_direction_left_to_right &
             blink::ContextMenuData::kCheckableMenuItemEnabled;

    default:
      return RenderViewContextMenu::IsCommandIdEnabled(command_id);
  }
}

ui::AcceleratorProvider*
RenderViewContextMenuViews::GetBrowserAcceleratorProvider() const {
  Browser* browser = GetBrowser();
  if (!browser)
    return nullptr;

  return BrowserView::GetBrowserViewForBrowser(browser);
}

void RenderViewContextMenuViews::AppendPlatformEditableItems() {
  bidi_submenu_model_.AddCheckItem(
      IDC_WRITING_DIRECTION_DEFAULT,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_DEFAULT));
  bidi_submenu_model_.AddCheckItem(
      IDC_WRITING_DIRECTION_LTR,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_LTR));
  bidi_submenu_model_.AddCheckItem(
      IDC_WRITING_DIRECTION_RTL,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_RTL));

  menu_model_.AddSubMenu(
      IDC_WRITING_DIRECTION_MENU,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_MENU),
      &bidi_submenu_model_);
}

void RenderViewContextMenuViews::Show() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return;

  // Menus need a Widget to work. If we're not the active tab we won't
  // necessarily be in a widget.
  views::Widget* top_level_widget = GetTopLevelWidget();
  if (!top_level_widget)
    return;

  // Don't show empty menus.
  if (menu_model().GetItemCount() == 0)
    return;

  // Convert from target window coordinates to root window coordinates.
  gfx::Point screen_point(params().x, params().y);
  aura::Window* target_window = GetActiveNativeView();
  aura::Window* root_window = target_window->GetRootWindow();
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(root_window);
  if (screen_position_client) {
    screen_position_client->ConvertPointToScreen(target_window, &screen_point);
  }
  // Enable recursive tasks on the message loop so we can get updates while
  // the context menu is being displayed.
  base::CurrentThread::ScopedNestableTaskAllower allow;
  RunMenuAt(top_level_widget, screen_point, params().source_type);

  auto* submenu_view = static_cast<ToolkitDelegateViews*>(toolkit_delegate())
                           ->menu_view()
                           ->GetSubmenu();
  if (submenu_view) {
    for (auto& observer : observers_) {
      if (submenu_view->host())
        observer.OnContextMenuShown(
            params_, submenu_view->host()->GetWindowBoundsInScreen());
    }

    submenu_view_observer_ =
        std::make_unique<SubmenuViewObserver>(this, submenu_view);
  }
}

views::Widget* RenderViewContextMenuViews::GetTopLevelWidget() {
  return views::Widget::GetTopLevelWidgetForNativeView(GetActiveNativeView());
}

aura::Window* RenderViewContextMenuViews::GetActiveNativeView() {
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(GetRenderFrameHost());
  if (!web_contents) {
    LOG(ERROR) << "RenderViewContextMenuViews::Show, couldn't find WebContents";
    return NULL;
  }
  return web_contents->GetNativeView();
}

void RenderViewContextMenuViews::OnSubmenuViewBoundsChanged(
    const gfx::Rect& new_bounds_in_screen) {
  for (auto& observer : observers_) {
    observer.OnContextMenuViewBoundsChanged(new_bounds_in_screen);
  }
}

void RenderViewContextMenuViews::OnSubmenuClosed() {
  submenu_view_observer_.reset();
}
