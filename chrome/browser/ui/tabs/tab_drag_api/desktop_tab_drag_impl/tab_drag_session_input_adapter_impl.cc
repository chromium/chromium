// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_drag_api/desktop_tab_drag_impl/tab_drag_session_input_adapter_impl.h"

#include "base/check.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "ui/base/base_window.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/views/event_monitor.h"

namespace {

base::expected<gfx::NativeWindow, mojo_base::mojom::ErrorPtr>
ResolveContextWindow(const std::vector<tabs_api::NodeId>& source_tab_ids) {
  CHECK(!source_tab_ids.empty());
  std::optional<tabs::TabHandle> tab_handle = source_tab_ids[0].ToTabHandle();
  if (!tab_handle.has_value()) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument, "invalid tab ID"));
  }
  tabs::TabInterface* tab_interface = tab_handle.value().Get();
  if (!tab_interface) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kNotFound, "tab no longer exists"));
  }
  BrowserWindowInterface* browser_window =
      tab_interface->GetBrowserWindowInterface();
  return browser_window->GetWindow()->GetNativeWindow();
}

}  // namespace

namespace tabs_api {

TabDragSessionInputAdapterImpl::TabDragSessionInputAdapterImpl() = default;
TabDragSessionInputAdapterImpl::~TabDragSessionInputAdapterImpl() = default;

base::expected<void, mojo_base::mojom::ErrorPtr>
TabDragSessionInputAdapterImpl::StartInputCapture(
    const std::vector<tabs_api::NodeId>& source_tab_ids,
    EventCallback callback) {
  auto context_result = ResolveContextWindow(source_tab_ids);
  if (!context_result.has_value()) {
    return base::unexpected(std::move(context_result.error()));
  }
  gfx::NativeWindow context = context_result.value();

  callback_ = std::move(callback);
  event_monitor_ = views::EventMonitor::CreateApplicationMonitor(
      this, context,
      {ui::EventType::kMouseMoved, ui::EventType::kMouseDragged,
       ui::EventType::kMouseReleased, ui::EventType::kKeyPressed});
  return base::ok();
}

void TabDragSessionInputAdapterImpl::ReleaseInputCapture() {
  event_monitor_.reset();
  callback_.Reset();
}

void TabDragSessionInputAdapterImpl::OnEvent(const ui::Event& event) {
  if (!callback_) {
    return;
  }

  if (event.type() == ui::EventType::kKeyPressed) {
    if (event.AsKeyEvent()->key_code() == ui::VKEY_ESCAPE) {
      callback_.Run({TabDragInputEvent::Type::kCancelled});
    }
  } else if (event.type() == ui::EventType::kMouseReleased) {
    callback_.Run({TabDragInputEvent::Type::kDropped,
                   event.AsMouseEvent()->root_location()});
  } else if (event.type() == ui::EventType::kMouseMoved ||
             event.type() == ui::EventType::kMouseDragged) {
    callback_.Run({TabDragInputEvent::Type::kMoved,
                   event.AsMouseEvent()->root_location()});
  }
}

}  // namespace tabs_api
