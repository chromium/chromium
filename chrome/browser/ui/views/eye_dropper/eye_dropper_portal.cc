// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/eye_dropper/eye_dropper_portal.h"

#include <algorithm>
#include <cmath>
#include <tuple>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/views/eye_dropper/eye_dropper.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/xdg/portal.h"
#include "components/dbus/xdg/portal_constants.h"
#include "components/dbus/xdg/request.h"
#include "content/public/browser/eye_dropper_listener.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/linux/linux_ui_delegate.h"

namespace {

constexpr char kScreenshotInterfaceName[] = "org.freedesktop.portal.Screenshot";
constexpr char kMethodPickColor[] = "PickColor";

gfx::AcceleratedWidget GetAcceleratedWidget(content::RenderFrameHost* frame) {
  if (!frame) {
    return gfx::kNullAcceleratedWidget;
  }
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame);
  if (!web_contents) {
    return gfx::kNullAcceleratedWidget;
  }
  gfx::NativeWindow top_level = web_contents->GetTopLevelNativeWindow();
  if (!top_level) {
    return gfx::kNullAcceleratedWidget;
  }
  auto* host = top_level->GetHost();
  if (!host) {
    return gfx::kNullAcceleratedWidget;
  }
  return host->GetAcceleratedWidget();
}

}  // namespace

// static
std::unique_ptr<content::EyeDropper> EyeDropperPortal::Create(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  return base::WrapUnique(new EyeDropperPortal(
      frame, listener, dbus_thread_linux::GetSharedSessionBus()));
}

// static
std::unique_ptr<content::EyeDropper> EyeDropperPortal::CreateForTesting(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener,
    scoped_refptr<dbus::Bus> bus) {
  return base::WrapUnique(
      new EyeDropperPortal(frame, listener, std::move(bus)));
}

EyeDropperPortal::EyeDropperPortal(content::RenderFrameHost* frame,
                                   content::EyeDropperListener* listener,
                                   scoped_refptr<dbus::Bus> bus)
    : listener_(listener), bus_(std::move(bus)) {
  if (!bus_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::WeakPtr<EyeDropperPortal> portal) {
                         if (portal) {
                           CHECK(portal->listener_);
                           portal->listener_->ColorSelectionCanceled();
                         }
                       },
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  gfx::AcceleratedWidget widget = GetAcceleratedWidget(frame);
  auto* delegate = ui::LinuxUiDelegate::GetInstance();
  if (widget != gfx::kNullAcceleratedWidget && delegate) {
    delegate->ExportWindowHandle(
        widget, base::BindOnce(&EyeDropperPortal::OnWindowHandleExported,
                               weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  OnWindowHandleExported(std::string());
}

EyeDropperPortal::~EyeDropperPortal() = default;

void EyeDropperPortal::OnWindowHandleExported(std::string handle) {
  parent_handle_ = std::move(handle);
  dbus_xdg::RequestXdgDesktopPortal(
      bus_.get(), base::BindOnce(&EyeDropperPortal::OnPortalServiceStarted,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void EyeDropperPortal::OnPortalServiceStarted(uint32_t version) {
  if (version == 0) {
    listener_->ColorSelectionCanceled();
    return;
  }

  auto* proxy =
      bus_->GetObjectProxy(dbus_xdg::kPortalServiceName,
                           dbus::ObjectPath(dbus_xdg::kPortalObjectPath));

  request_ = std::make_unique<dbus_xdg::Request>(
      bus_, proxy, kScreenshotInterfaceName, kMethodPickColor,
      dbus_xdg::Dictionary(),
      base::BindOnce(&EyeDropperPortal::OnResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      parent_handle_);
}

void EyeDropperPortal::OnResponse(dbus_xdg::Results results) {
  if (!results.has_value()) {
    listener_->ColorSelectionCanceled();
    return;
  }

  auto it = results->find("color");
  if (it == results->end()) {
    listener_->ColorSelectionCanceled();
    return;
  }

  auto color_opt =
      std::move(it->second).Take<std::tuple<double, double, double>>();
  if (!color_opt) {
    listener_->ColorSelectionCanceled();
    return;
  }

  auto [r, g, b] = *color_opt;
  uint8_t r_byte = std::clamp(static_cast<int>(std::lround(r * 255.0)), 0, 255);
  uint8_t g_byte = std::clamp(static_cast<int>(std::lround(g * 255.0)), 0, 255);
  uint8_t b_byte = std::clamp(static_cast<int>(std::lround(b * 255.0)), 0, 255);
  listener_->ColorSelected(SkColorSetRGB(r_byte, g_byte, b_byte));
}
