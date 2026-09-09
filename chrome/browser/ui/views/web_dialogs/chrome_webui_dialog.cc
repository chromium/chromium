// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_dialogs/chrome_webui_dialog.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace webui_dialog {

namespace {

// EnableSizingFromWebContents() DCHECKs on an empty maximum, so an
// unconstrained dimension is given to the renderer as the widest range it can
// express. ResizeDueToAutoResize() still clamps the result to the work area.
constexpr int kUnboundedExtent = std::numeric_limits<int>::max();

gfx::Size EffectiveMinSize(const WebDialogSpec& spec) {
  // Auto-resize does not accept a zero-sized minimum.
  return gfx::Size(std::max(1, spec.min_size.width()),
                   std::max(1, spec.min_size.height()));
}

gfx::Size EffectiveMaxSize(const WebDialogSpec& spec) {
  return gfx::Size(
      spec.max_size.width() > 0 ? spec.max_size.width() : kUnboundedExtent,
      spec.max_size.height() > 0 ? spec.max_size.height() : kUnboundedExtent);
}

void MaybeShowWidget(views::Widget* widget, bool activate) {
  CHECK(widget);
  if (activate) {
    widget->Show();
  } else {
    widget->ShowInactive();
  }
}

}  // namespace

WebDialogSpec::WebDialogSpec() = default;
WebDialogSpec::~WebDialogSpec() = default;
WebDialogSpec::WebDialogSpec(const WebDialogSpec&) = default;

// static
std::unique_ptr<views::Widget> ChromeWebUIDialog::Show(
    gfx::NativeWindow parent,
    std::unique_ptr<WebUIContentsWrapper> contents_wrapper,
    const WebDialogSpec& spec) {
  auto dialog =
      std::make_unique<ChromeWebUIDialog>(std::move(contents_wrapper), spec);
  ChromeWebUIDialog* dialog_ptr = dialog.get();

  std::unique_ptr<views::Widget> widget;

  if (spec.modal_type == ui::mojom::ModalType::kChild) {
    CHECK(spec.parent_tab)
        << "kChild (tab-modal) dialogs require spec.parent_tab";
    // Only the TabDialogManager makes a dialog genuinely tab-modal: it syncs
    // visibility with the tab, closes on navigate and detach, suppresses input
    // on the page underneath, and maintains CanShowModalUI(). A bare child
    // widget looks modal and blocks nothing.
    tabs::TabFeatures* features = spec.parent_tab->GetTabFeatures();
    tabs::TabDialogManager* manager =
        features ? features->tab_dialog_manager() : nullptr;
    CHECK(manager) << "kChild dialogs require a tab with a TabDialogManager";
    widget = manager->CreateTabScopedDialog(dialog.release());
  } else if (spec.modal_type == ui::mojom::ModalType::kWindow ||
             spec.modal_type == ui::mojom::ModalType::kSystem) {
    widget = base::WrapUnique(constrained_window::CreateBrowserModalDialogViews(
        dialog.release(), parent));
  } else {
    // Allows for non-modal unanchored dialog options.
    widget = base::WrapUnique(views::DialogDelegate::CreateDialogWidget(
        dialog.release(), /*context=*/parent,
        /*parent=*/gfx::NativeView()));
  }

  // Observe the widget so the delegate can safely clean itself up.
  dialog_ptr->widget_observation_.Observe(widget.get());

  dialog_ptr->UpdateAutoResizeBounds();

  if (!spec.wait_for_explicit_show) {
    dialog_ptr->ShowWidget();
  }

  return widget;
}

ChromeWebUIDialog::ChromeWebUIDialog(
    std::unique_ptr<WebUIContentsWrapper> contents_wrapper,
    const WebDialogSpec& spec)
    : spec_(spec), contents_wrapper_(std::move(contents_wrapper)) {
  SetOwnershipOfNewWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  SetButtons(spec_.buttons);
  SetModalType(spec_.modal_type);
  SetShowCloseButton(spec_.show_close_button);
  SetHasWindowSizeControls(spec_.has_window_size_controls);
  set_esc_should_cancel_dialog_override(
      spec_.esc_should_cancel_dialog_override);

  set_margins(gfx::Insets());

  auto web_view = std::make_unique<views::WebView>(
      contents_wrapper_->web_contents()->GetBrowserContext());
  web_view_ = web_view.get();
  web_view_->SetWebContents(contents_wrapper_->web_contents());

  // Unless it was preloaded the WebContents is created hidden, and a hidden
  // renderer produces no frames: no auto-resize would arrive and the page
  // would never reach the point of calling ShowUI().
  contents_wrapper_->web_contents()->WasShown();
  // Let the dialog handle accelerators the page declines, notably ESC.
  web_view_->set_allow_accelerators(true);
  if (spec_.element_identifier) {
    web_view_->SetProperty(views::kElementIdentifierKey,
                           spec_.element_identifier);
  }

  // Establish an initial preferred size. Mac does not support zero-sized
  // windows, and other platforms may hit DCHECKs during Widget::Init if bounds
  // are empty.
  gfx::Size initial_size = spec_.min_size;
  if (initial_size.IsEmpty()) {
    initial_size = gfx::Size(1, 1);
  }
  web_view_->SetPreferredSize(initial_size);
  UpdateAutoResizeBounds();

  view_observation_.Observe(web_view_);

  // Nested rather than being the contents view directly: a View holds at most
  // one element identifier and the dialog needs one of its own.
  auto contents_container = std::make_unique<views::View>();
  contents_container->SetUseDefaultFillLayout(true);
  if (spec_.dialog_element_identifier) {
    contents_container->SetProperty(views::kElementIdentifierKey,
                                    spec_.dialog_element_identifier);
  }
  contents_container->AddChildView(std::move(web_view));
  SetContentsView(std::move(contents_container));

  DCHECK(!contents_wrapper_->GetHost())
      << "The contents wrapper already has a host";
  contents_wrapper_->SetHost(weak_ptr_factory_.GetWeakPtr());
}

ChromeWebUIDialog::~ChromeWebUIDialog() = default;

views::View* ChromeWebUIDialog::GetInitiallyFocusedView() {
  return web_view_;
}

void ChromeWebUIDialog::ShowUI() {
  ShowWidget();
}

void ChromeWebUIDialog::ShowWidget() {
  views::Widget* widget = GetWidget();
  if (!widget) {
    return;
  }

  if (spec_.modal_type == ui::mojom::ModalType::kChild) {
    if (!spec_.parent_tab || widget->IsClosed()) {
      return;
    }

    tabs::TabDialogManager* manager =
        spec_.parent_tab->GetTabFeatures()->tab_dialog_manager();
    // The TabDialogManager owns visibility once the dialog is registered, so
    // Widget::Show() would bypass its bookkeeping and a repeat ShowUI() must
    // not register twice.
    if (manager->IsDialogManaged(widget)) {
      return;
    }
    manager->ShowDialog(widget,
                        std::make_unique<tabs::TabDialogManager::Params>());
    return;
  }

  // Note: On some platforms (such as Mac or Wayland), the widget's visibility
  // state is updated asynchronously.
  if (!widget->IsVisible()) {
    MaybeShowWidget(widget, spec_.activate_on_show);
  }
}

void ChromeWebUIDialog::CloseUI() {
  if (GetWidget()) {
    GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
}

void ChromeWebUIDialog::ResizeDueToAutoResize(content::WebContents* source,
                                              const gfx::Size& new_size) {
  // OnViewIsDeleting() can clear `web_view_` while the wrapper still holds its
  // host reference.
  if (!web_view_) {
    return;
  }

  gfx::Size bounded_size = new_size;

  // The `new_size` comes from the renderer, which is instructed to respect
  // the min/max bounds by UpdateAutoResizeBounds(). However, the size must be
  // clamped here to guarantee strict adherence to `spec_`.
  bounded_size.SetToMax(EffectiveMinSize(spec_));
  bounded_size.SetToMin(EffectiveMaxSize(spec_));

  // WebUIContentsWrapper::SetHost() delivers the frame's current size, and the
  // constructor calls it before there is a widget. Record the size so the
  // widget is created with it rather than opening at min_size.
  if (!GetWidget()) {
    web_view_->SetPreferredSize(bounded_size);
    return;
  }

  // Ensure that the bounds are updated so that they can be used for the page
  // load.
  UpdateAutoResizeBounds();

  if (std::optional<gfx::Size> max_content = MaxContentSizeForWorkArea()) {
    bounded_size.SetToMin(*max_content);
  }

  web_view_->SetPreferredSize(bounded_size);

  // A tab-modal dialog is positioned relative to its tab by the
  // TabDialogManager, so centering it in the browser window would drag it off
  // that anchor on every resize.
  if (spec_.modal_type == ui::mojom::ModalType::kChild) {
    if (spec_.parent_tab) {
      tabs::TabDialogManager* manager =
          spec_.parent_tab->GetTabFeatures()->tab_dialog_manager();
      if (manager->IsDialogManaged(GetWidget())) {
        manager->UpdateModalDialogBounds();
      }
    }
    return;
  }

  // Resize the widget to fit the new preferred size of the contents.
  // The non-client view includes the window frame, so this ensures the
  // entire dialog is sized correctly.
  GetWidget()->CenterWindow(GetWidget()->non_client_view()->GetPreferredSize());
}

std::optional<gfx::Size> ChromeWebUIDialog::MaxContentSizeForWorkArea() {
  views::Widget* widget = GetWidget();
  if (!widget) {
    return std::nullopt;
  }

  const gfx::Rect work_area = widget->GetWorkAreaBoundsInScreen();
  if (work_area.IsEmpty()) {
    return std::nullopt;
  }

  // The work area has to host the whole window, so what is left for the web
  // contents is the work area less the frame around it.
  gfx::Size frame_size;
  if (widget->non_client_view()) {
    frame_size = widget->non_client_view()
                     ->GetWindowBoundsForClientBounds(gfx::Rect())
                     .size();
  }

  gfx::Size max_content(std::max(0, work_area.width() - frame_size.width()),
                        std::max(0, work_area.height() - frame_size.height()));
  // Auto-resize rejects a zero-sized extent and keeps the bounds usable even
  // for a degenerate work area.
  max_content.SetToMax(gfx::Size(1, 1));
  return max_content;
}

void ChromeWebUIDialog::UpdateAutoResizeBounds() {
  // OnViewIsDeleting() can clear `web_view_` while the wrapper still holds its
  // host reference.
  if (!web_view_) {
    return;
  }

  gfx::Size capped_min = EffectiveMinSize(spec_);
  gfx::Size capped_max = EffectiveMaxSize(spec_);

  if (std::optional<gfx::Size> max_content = MaxContentSizeForWorkArea()) {
    capped_max.SetToMin(*max_content);
  }

  capped_min.SetToMin(capped_max);
  web_view_->EnableSizingFromWebContents(capped_min, capped_max);
}

bool ChromeWebUIDialog::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  views::Widget* widget = GetWidget();
  // GetFocusManager() is null until the widget has a top-level, and
  // UnhandledKeyboardEventHandler CHECKs it.
  views::FocusManager* focus_manager =
      widget ? widget->GetFocusManager() : nullptr;
  if (!focus_manager) {
    return false;
  }

  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(event,
                                                               focus_manager);
}

content::WebContents* ChromeWebUIDialog::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  if (!spec_.add_new_contents_callback) {
    return nullptr;
  }

  return spec_.add_new_contents_callback.Run(source, std::move(new_contents),
                                             target_url, disposition,
                                             window_features, user_gesture);
}

void ChromeWebUIDialog::OnViewAddedToWidget(views::View* observed_view) {
  if (observed_view != web_view_) {
    return;
  }

  // Apply rounded corners to the NativeViewHost to prevent WebUI content
  // from bleeding through the dialog's rounded corners.
  //
  // TODO(https://crbug.com/344626785): Remove this once DialogDelegate
  // natively supports rounded corners.
  web_view_->holder()->SetNativeViewCornerRadii(
      gfx::RoundedCornersF(spec_.corner_radius.value_or(GetCornerRadius())));

  UpdateAutoResizeBounds();
}

void ChromeWebUIDialog::OnViewIsDeleting(views::View* observed_view) {
  if (observed_view == web_view_) {
    view_observation_.Reset();
    web_view_ = nullptr;
  }
}

void ChromeWebUIDialog::OnWidgetBoundsChanged(views::Widget* widget,
                                              const gfx::Rect& new_bounds) {
  UpdateAutoResizeBounds();
}

void ChromeWebUIDialog::OnDisplayMetricsChanged(const display::Display& display,
                                                uint32_t changed_metrics) {
  constexpr uint32_t kSizeAffectingMetrics =
      display::DisplayObserver::DISPLAY_METRIC_BOUNDS |
      display::DisplayObserver::DISPLAY_METRIC_WORK_AREA |
      display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
      display::DisplayObserver::DISPLAY_METRIC_ROTATION;
  if (changed_metrics & kSizeAffectingMetrics) {
    UpdateAutoResizeBounds();
  }
}

void ChromeWebUIDialog::OnWidgetDestroyed(views::Widget* widget) {
  widget_observation_.Reset();
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

}  // namespace webui_dialog
