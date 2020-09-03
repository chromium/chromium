// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"

#include <algorithm>
#include <list>

#include "ash/public/cpp/shell_window_ids.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/chrome_web_dialog_view.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"

namespace chromeos {

namespace {

// Track all open system web dialog instances. This should be a small list.
std::list<SystemWebDialogDelegate*>* GetInstances() {
  static base::NoDestructor<std::list<SystemWebDialogDelegate*>> instances;
  return instances.get();
}

}  // namespace

// static
const size_t SystemWebDialogDelegate::kDialogMarginForInternalScreenPx = 48;

// static
SystemWebDialogDelegate* SystemWebDialogDelegate::FindInstance(
    const std::string& id) {
  auto* instances = GetInstances();
  auto iter =
      std::find_if(instances->begin(), instances->end(),
                   [id](SystemWebDialogDelegate* i) { return i->Id() == id; });
  return iter == instances->end() ? nullptr : *iter;
}

// static
bool SystemWebDialogDelegate::HasInstance(const GURL& url) {
  auto* instances = GetInstances();
  auto it = std::find_if(
      instances->begin(), instances->end(),
      [url](SystemWebDialogDelegate* dialog) { return dialog->gurl_ == url; });
  return it != instances->end();
}

// static
gfx::Size SystemWebDialogDelegate::ComputeDialogSizeForInternalScreen(
    const gfx::Size& preferred_size) {
  // If the device has no internal display (e.g., for Chromeboxes), use the
  // preferred size.
  // TODO(https://crbug.com/1035060): It could be possible that a Chromebox is
  // hooked up to a low-resolution monitor. It might be a good idea to check
  // that display's resolution as well.
  if (!display::Display::HasInternalDisplay())
    return preferred_size;

  // According to the Chrome OS dialog spec, dialogs should have a 48px margin
  // from the edge of an internal display.
  static const gfx::Insets margins =
      gfx::Insets(kDialogMarginForInternalScreenPx);

  display::Display internal_display;
  if (!display::Screen::GetScreen()->GetDisplayWithDisplayId(
          display::Display::InternalDisplayId(), &internal_display)) {
    NOTREACHED() << "Could not fetch metadata for internal display.";
  }

  // Work area size does not include the status bar.
  gfx::Size work_area_size = internal_display.work_area_size();

  // The max width possible is the screen's width adjusted by the left/right
  // margins.
  int max_work_area_width =
      work_area_size.width() - margins.left() - margins.right();

  // The max height possible is the screen's height adjusted by the top/bottom
  // margins.
  int max_work_area_height =
      work_area_size.height() - margins.top() - margins.bottom();

  // Take the minimum of the preferred size and the max size.
  return gfx::Size(std::min({preferred_size.width(), max_work_area_width}),
                   std::min({preferred_size.height(), max_work_area_height}));
}

SystemWebDialogDelegate::SystemWebDialogDelegate(const GURL& gurl,
                                                 const base::string16& title)
    : gurl_(gurl), title_(title), modal_type_(ui::MODAL_TYPE_NONE) {
  set_can_resize(false);
  switch (session_manager::SessionManager::Get()->session_state()) {
    // Normally system dialogs are not modal.
    case session_manager::SessionState::UNKNOWN:
    case session_manager::SessionState::LOGGED_IN_NOT_ACTIVE:
    case session_manager::SessionState::ACTIVE:
      break;
    // These states use an overlay so dialogs must be modal.
    case session_manager::SessionState::OOBE:
    case session_manager::SessionState::LOGIN_PRIMARY:
    case session_manager::SessionState::LOCKED:
    case session_manager::SessionState::LOGIN_SECONDARY:
      modal_type_ = ui::MODAL_TYPE_SYSTEM;
      break;
  }
  GetInstances()->push_back(this);
}

SystemWebDialogDelegate::~SystemWebDialogDelegate() {
  base::EraseIf(*GetInstances(),
                [this](SystemWebDialogDelegate* i) { return i == this; });
}

const std::string& SystemWebDialogDelegate::Id() {
  return gurl_.spec();
}

void SystemWebDialogDelegate::Focus() {
  // Focusing a modal dialog does not make it the topmost dialog and does not
  // enable interaction. It does however remove focus from the current dialog,
  // preventing interaction with any dialog. TODO(stevenjb): Investigate and
  // fix, https://crbug.com/914133.
  if (modal_type_ == ui::MODAL_TYPE_NONE)
    dialog_window()->Focus();
}

void SystemWebDialogDelegate::Close() {
  DCHECK(dialog_window());
  views::Widget::GetWidgetForNativeWindow(dialog_window())->Close();
}

ui::ModalType SystemWebDialogDelegate::GetDialogModalType() const {
  return modal_type_;
}

base::string16 SystemWebDialogDelegate::GetDialogTitle() const {
  return title_;
}

GURL SystemWebDialogDelegate::GetDialogContentURL() const {
  return gurl_;
}

void SystemWebDialogDelegate::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {}

void SystemWebDialogDelegate::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDialogWidth, kDialogHeight);
}

std::string SystemWebDialogDelegate::GetDialogArgs() const {
  return std::string();
}

void SystemWebDialogDelegate::OnDialogShown(content::WebUI* webui) {
  webui_ = webui;

  // System dialogs don't use the browser's default page zoom. Their contents
  // stay at 100% to match the size of app list, shelf, status area, etc.
  auto* web_contents = webui_->GetWebContents();
  auto* rvh = web_contents->GetRenderViewHost();
  auto* zoom_map = content::HostZoomMap::GetForWebContents(web_contents);
  // Temporary means the lifetime of the WebContents.
  zoom_map->SetTemporaryZoomLevel(rvh->GetProcess()->GetID(),
                                  rvh->GetRoutingID(),
                                  blink::PageZoomFactorToZoomLevel(1.0));
}

void SystemWebDialogDelegate::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void SystemWebDialogDelegate::OnCloseContents(content::WebContents* source,
                                              bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool SystemWebDialogDelegate::ShouldShowDialogTitle() const {
  return !title_.empty();
}

void SystemWebDialogDelegate::ShowSystemDialogForBrowserContext(
    content::BrowserContext* browser_context,
    gfx::NativeWindow parent) {
  views::Widget::InitParams extra_params;
  // If unparented and not modal, keep it on top (see header comment).
  if (!parent && GetDialogModalType() == ui::MODAL_TYPE_NONE)
    extra_params.z_order = ui::ZOrderLevel::kFloatingWindow;
  AdjustWidgetInitParams(&extra_params);
  dialog_window_ = chrome::ShowWebDialogWithParams(
      parent, browser_context, this,
      base::make_optional<views::Widget::InitParams>(std::move(extra_params)));
}

void SystemWebDialogDelegate::ShowSystemDialog(gfx::NativeWindow parent) {
  ShowSystemDialogForBrowserContext(ProfileManager::GetActiveUserProfile(),
                                    parent);
}
}  // namespace chromeos
