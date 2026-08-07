// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_drag_api/desktop_tab_drag_impl/tab_drag_window_adapter_impl.h"

#include "base/notimplemented.h"
#include "base/scoped_observation.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_manager.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_window_registry.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "ui/base/base_window.h"
#include "ui/display/screen.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

TabDragWindowAdapterImpl::TabDragWindowAdapterImpl(
    BrowserWindowInterface* browser_window)
    : browser_window_(browser_window), registry_(nullptr) {
  if (auto* manager =
          g_browser_process->GetFeatures()->tab_drag_session_manager()) {
    registry_ = manager->GetWindowRegistry();
    id_ = registry_->Register(this);
  }
}

TabDragWindowAdapterImpl::~TabDragWindowAdapterImpl() {
  if (registry_) {
    registry_->Unregister(id_);
  }
}

tabs_api::TabDragWindowId TabDragWindowAdapterImpl::GetWindowId() const {
  return id_;
}

gfx::NativeWindow TabDragWindowAdapterImpl::GetNativeWindow() const {
  return browser_window_->GetWindow()->GetNativeWindow();
}

gfx::Rect TabDragWindowAdapterImpl::GetBoundsInScreen() const {
  return browser_window_->GetWindow()->GetBounds();
}

bool TabDragWindowAdapterImpl::IsDraggingEntireWindow(
    size_t dragged_tab_count) const {
  return browser_window_ &&
         dragged_tab_count ==
             static_cast<size_t>(browser_window_->GetTabStripModel()->count());
}

namespace {

// Helper to recursively find the NativeViewHost for a given NativeView.
// Returning nullptr means it is not found.
views::View* FindNativeViewHost(views::View* root,
                                gfx::NativeView native_view) {
  if (auto* host = views::AsViewClass<views::NativeViewHost>(root)) {
    if (host->native_view() == native_view) {
      return host;
    }
  }
  for (views::View* child : root->children()) {
    if (auto* found = FindNativeViewHost(child, native_view)) {
      return found;
    }
  }
  return nullptr;
}

}  // namespace

gfx::Point TabDragWindowAdapterImpl::ConvertScreenPointToLocal(
    gfx::NativeView target_view,
    const gfx::Point& screen_point) const {
  gfx::Point local_point = screen_point;
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      browser_window_->GetWindow()->GetNativeWindow());
  CHECK(widget);
  CHECK(widget->GetRootView());

  views::View* host = FindNativeViewHost(widget->GetRootView(), target_view);
  CHECK(host) << "Target view not found in window hierarchy";

  views::View::ConvertPointFromScreen(host, &local_point);
  return local_point;
}

void TabDragWindowAdapterImpl::SetCapture() {
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      browser_window_->GetWindow()->GetNativeWindow());
  if (widget) {
    widget->SetCapture(nullptr);
  }
}

void TabDragWindowAdapterImpl::ReleaseCapture() {
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      browser_window_->GetWindow()->GetNativeWindow());
  if (widget) {
    widget->ReleaseCapture();
  }
}

bool TabDragWindowAdapterImpl::HasCapture() const {
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      browser_window_->GetWindow()->GetNativeWindow());
  return widget && widget->HasCapture();
}

void TabDragWindowAdapterImpl::Activate() {
  if (!browser_window_ || !browser_window_->GetWindow()) {
    return;
  }
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      browser_window_->GetWindow()->GetNativeWindow());
  if (widget) {
    widget->Activate();
  }
}

base::expected<tabs_api::TabDragWindowId, mojo_base::mojom::ErrorPtr>
TabDragWindowAdapterImpl::DetachToNewWindow(
    const std::vector<tabs_api::NodeId>& tab_ids,
    const gfx::Point& screen_point,
    const gfx::Vector2d& drag_offset) {
  if (!browser_window_) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kFailedPrecondition,
        "Browser window interface is null"));
  }

  Profile* profile = browser_window_->GetProfile();
  if (!profile) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kFailedPrecondition, "Profile is null"));
  }

  gfx::Rect initial_bounds(screen_point - drag_offset,
                           browser_window_->GetWindow()->GetBounds().size());

  BrowserWindowCreateParams params(BrowserWindowInterface::Type::TYPE_NORMAL,
                                   *profile,
                                   /*from_user_gesture=*/true);
  params.initial_bounds = initial_bounds;

  BrowserWindowInterface* new_window = CreateBrowserWindow(std::move(params));
  if (!new_window) {
    return base::unexpected(
        mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInternal,
                                     "Failed to create new browser window"));
  }

  CHECK(registry_);
  gfx::NativeWindow native_window = new_window->GetWindow()->GetNativeWindow();
  tabs_api::TabDragWindowId new_window_id =
      registry_->FindWindowIdByNativeWindow(native_window);
  if (!new_window_id) {
    new_window->GetWindow()->Close();
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInternal, "New window was not registered"));
  }

  auto migration_result = MigrateTabs(new_window_id, tab_ids);
  if (!migration_result.has_value()) {
    new_window->GetWindow()->Close();
    return base::unexpected(std::move(migration_result.error()));
  }

  new_window->GetWindow()->Show();
  return new_window_id;
}

tabs_api::DragMoveLoopResult TabDragWindowAdapterImpl::RunWindowMoveLoop(
    const gfx::Point& screen_point,
    const gfx::Vector2d& drag_offset,
    tabs_api::TabDragWindowAdapter::WindowMoveCallback move_callback) {
  if (!browser_window_) {
    return tabs_api::DragMoveLoopResult::kCanceled;
  }
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      browser_window_->GetWindow()->GetNativeWindow());
  if (!widget) {
    return tabs_api::DragMoveLoopResult::kCanceled;
  }

  // Force the window to be positioned relative to the mouse using the offset,
  // overriding any window manager positioning.
  gfx::Rect bounds = widget->GetWindowBoundsInScreen();
  gfx::Point new_origin = screen_point - drag_offset;
  widget->SetBounds(gfx::Rect(new_origin, bounds.size()));

  move_callback_ = std::move(move_callback);

  base::ScopedObservation<views::Widget, views::WidgetObserver> observation(
      this);
  observation.Observe(widget);

  views::Widget::MoveLoopResult result =
      widget->RunMoveLoop(drag_offset, views::Widget::MoveLoopSource::kMouse,
                          views::Widget::MoveLoopEscapeBehavior::kHide);

  move_callback_.Reset();

  return result == views::Widget::MoveLoopResult::kSuccessful
             ? tabs_api::DragMoveLoopResult::kSuccess
             : tabs_api::DragMoveLoopResult::kCanceled;
}

void TabDragWindowAdapterImpl::EndWindowMoveLoop() {
  if (!browser_window_) {
    return;
  }
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      browser_window_->GetWindow()->GetNativeWindow());
  if (widget) {
    widget->EndMoveLoop();
  }
}

base::expected<void, mojo_base::mojom::ErrorPtr>
TabDragWindowAdapterImpl::MigrateTabs(
    tabs_api::TabDragWindowId target_window_id,
    const std::vector<tabs_api::NodeId>& tab_ids) {
  if (!registry_) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kFailedPrecondition, "Registry is null"));
  }

  tabs_api::TabDragWindowAdapter* target_adapter =
      registry_->Get(target_window_id);
  if (!target_adapter) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kNotFound, "Target window not found"));
  }

  gfx::NativeWindow target_native_window = target_adapter->GetNativeWindow();
  BrowserWindowInterface* target_window =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithWindow(
          target_native_window);

  BrowserWindowInterface* source_window = browser_window_;

  if (!source_window) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument, "Source window is null"));
  }
  if (!target_window) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument, "Target window is null"));
  }
  TabStripModel* source_model = source_window->GetTabStripModel();
  if (!source_model) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kFailedPrecondition,
        "Source tab strip model is null"));
  }
  TabStripModel* target_model = target_window->GetTabStripModel();
  if (!target_model) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kFailedPrecondition,
        "Target tab strip model is null"));
  }

  for (const auto& node_id : tab_ids) {
    if (node_id.Type() != tabs_api::NodeId::Type::kContent) {
      continue;  // Only support content nodes (tabs) for now.
    }

    auto tab_handle_opt = node_id.ToTabHandle();
    if (!tab_handle_opt) {
      return base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kInvalidArgument, "Invalid tab handle"));
    }

    tabs::TabInterface* tab_interface = tab_handle_opt->Get();
    if (!tab_interface) {
      return base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kNotFound, "Tab not found"));
    }

    int index = source_model->GetIndexOfTab(tab_interface);
    if (index == TabStripModel::kNoTab) {
      return base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kNotFound, "Tab not in source window"));
    }

    std::unique_ptr<tabs::TabModel> detached_tab =
        source_model->DetachTabAtForInsertion(index);
    if (!detached_tab) {
      return base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kInternal, "Failed to detach tab"));
    }

    int add_types = AddTabTypes::ADD_NONE;
    if (target_model->empty() || node_id == tab_ids.front()) {
      add_types |= AddTabTypes::ADD_ACTIVE;
    }

    target_model->InsertDetachedTabAt(target_model->count(),
                                      std::move(detached_tab), add_types);
  }

  return base::ok();
}

void TabDragWindowAdapterImpl::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  if (move_callback_) {
    move_callback_.Run(display::Screen::Get()->GetCursorScreenPoint());
  }
}
