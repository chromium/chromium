// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_icon_linux_x11.h"

#include <X11/X.h>
#include <X11/Xlib.h>

#include <limits>
#include <memory>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/shell_integration_linux.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_error_tracker.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/views/background.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"

namespace {

constexpr long kSystemTrayRequestDock = 0;

constexpr int kXembedInfoProtocolVersion = 0;
constexpr int kXembedFlagMap = 1 << 0;
constexpr int kXembedInfoFlags = kXembedFlagMap;

const int16_t kInitialWindowPos = std::numeric_limits<int16_t>::min();

}  // namespace

StatusIconLinuxX11::StatusIconLinuxX11() : Button(this) {}

StatusIconLinuxX11::~StatusIconLinuxX11() = default;

void StatusIconLinuxX11::SetIcon(const gfx::ImageSkia& image) {
  SchedulePaint();
}

void StatusIconLinuxX11::SetToolTip(const base::string16& tool_tip) {
  SetTooltipText(tool_tip);
}

void StatusIconLinuxX11::UpdatePlatformContextMenu(ui::MenuModel* model) {
  // Nothing to do.
}

void StatusIconLinuxX11::OnSetDelegate() {
  XDisplay* const display = gfx::GetXDisplay();
  std::string atom_name =
      "_NET_SYSTEM_TRAY_S" + base::NumberToString(DefaultScreen(display));
  XID manager = XGetSelectionOwner(display, gfx::GetAtom(atom_name.c_str()));
  if (manager == x11::None) {
    delegate_->OnImplInitializationFailed();
    // |this| may be destroyed!
    return;
  }

  widget_ = std::make_unique<views::Widget>();

  auto native_widget =
      std::make_unique<views::DesktopNativeWidgetAura>(widget_.get());

  auto host = std::make_unique<views::DesktopWindowTreeHostX11>(
      widget_.get(), native_widget.get());
  host_ = host.get();

  // We outlive the host, so no need to remove ourselves as an observer.
  host->AddObserver(this);

  int visual_id;
  if (ui::GetIntProperty(manager, "_NET_SYSTEM_TRAY_VISUAL", &visual_id))
    host->SetPendingXVisualId(visual_id);

  const int width = std::max(1, delegate_->GetImage().width());
  const int height = std::max(1, delegate_->GetImage().height());

  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_CONTROL;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.remove_standard_frame = true;
  params.bounds =
      gfx::Rect(kInitialWindowPos, kInitialWindowPos, width, height);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.native_widget = native_widget.release();
  params.desktop_window_tree_host = host.release();
  params.wm_class_name = shell_integration_linux::GetProgramClassName();
  params.wm_class_class = shell_integration_linux::GetProgramClassClass();
  // The status icon is a tiny window that doesn't update very often, so
  // creating a compositor would only be wasteful of resources.
  params.force_software_compositing = true;

  widget_->Init(std::move(params));

  Window window = host_->GetAcceleratedWidget();
  DCHECK(window);

  ui::SetIntArrayProperty(window, "_XEMBED_INFO", "CARDINAL",
                          {kXembedInfoProtocolVersion, kXembedInfoFlags});

  XSetWindowAttributes attrs;
  unsigned long flags = 0;
  if (widget_->ShouldWindowContentsBeTransparent()) {
    flags |= CWBackPixel;
    attrs.background_pixel = 0;
  } else {
    ui::SetIntProperty(window, "CHROMIUM_COMPOSITE_WINDOW", "CARDINAL", 1);
    flags |= CWBackPixmap;
    attrs.background_pixmap = ParentRelative;
  }
  XChangeWindowAttributes(display, window, flags, &attrs);

  widget_->SetContentsView(this);
  set_owned_by_client();

  SetBorder(nullptr);
  SetIcon(delegate_->GetImage());
  SetTooltipText(delegate_->GetToolTip());
  set_context_menu_controller(this);

  XEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.xclient.type = ClientMessage;
  ev.xclient.window = manager;
  ev.xclient.message_type = gfx::GetAtom("_NET_SYSTEM_TRAY_OPCODE");
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = ui::X11EventSource::GetInstance()->GetTimestamp();
  ev.xclient.data.l[1] = kSystemTrayRequestDock;
  ev.xclient.data.l[2] = window;

  bool error;
  {
    gfx::X11ErrorTracker error_tracker;
    XSendEvent(display, manager, false, NoEventMask, &ev);
    error = error_tracker.FoundNewError();
  }
  if (error) {
    delegate_->OnImplInitializationFailed();
    // |this| may be destroyed!
  }
}

void StatusIconLinuxX11::ShowContextMenuForViewImpl(
    View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  ui::MenuModel* menu = delegate_->GetMenuModel();
  if (!menu)
    return;
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu, views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU |
                views::MenuRunner::FIXED_ANCHOR);
  menu_runner_->RunMenuAt(widget_.get(), nullptr, gfx::Rect(point, gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

void StatusIconLinuxX11::ButtonPressed(Button* sender, const ui::Event& event) {
  delegate_->OnClick();
}

void StatusIconLinuxX11::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  canvas->UndoDeviceScaleFactor();

  gfx::Rect bounds = host_->GetBoundsInPixels();
  const gfx::ImageSkia& image = delegate_->GetImage();

  // If the image fits in the window, center it.  But if it won't fit, downscale
  // it preserving aspect ratio.
  float scale =
      std::min({1.0f, static_cast<float>(bounds.width()) / image.width(),
                static_cast<float>(bounds.height()) / image.height()});
  float x_offset = (bounds.width() - scale * image.width()) / 2.0f;
  float y_offset = (bounds.height() - scale * image.height()) / 2.0f;

  gfx::Transform transform;
  transform.Translate(x_offset, y_offset);
  transform.Scale(scale, scale);
  canvas->Transform(transform);

  cc::PaintFlags flags;
  flags.setFilterQuality(kHigh_SkFilterQuality);
  canvas->DrawImageInt(image, 0, 0, image.width(), image.height(), 0, 0,
                       image.width(), image.height(), true, flags);
}

void StatusIconLinuxX11::OnWindowMapped(unsigned long xid) {
  // The window gets mapped by the system tray implementation.  Show() the
  // window (which will be a no-op) so aura is convinced the window is mapped
  // and will begin drawing frames.
  widget_->Show();
}

void StatusIconLinuxX11::OnWindowUnmapped(unsigned long xid) {}
