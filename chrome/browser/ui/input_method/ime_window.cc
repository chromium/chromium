// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/input_method/ime_window.h"

#include <utility>

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/input_method/ime_native_window.h"
#include "chrome/browser/ui/input_method/ime_window_observer.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "ui/display/display.h"
#include "ui/display/display_finder.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image.h"

namespace {

// The vertical margin between the cursor and the follow-cursor window.
const int kFollowCursorMargin = 3;

// The offset from the left of follow cursor window to the left of cursor.
const int kFollowCursorOffset = 32;

}  // namespace

namespace ui {

ImeWindow::ImeWindow(Profile* profile,
                     const extensions::Extension* extension,
                     content::RenderFrameHost* opener_render_frame_host,
                     const std::string& url,
                     Mode mode,
                     const gfx::Rect& bounds)
    : mode_(mode), native_window_(nullptr) {
  if (extension) {  // Allow nullable |extension| for testability.
    title_ = extension->name();
    icon_ = std::make_unique<extensions::IconImage>(
        profile, extension, extensions::IconsInfo::GetIcons(extension),
        extension_misc::EXTENSION_ICON_BITTY, gfx::ImageSkia(), this);
  }

  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());

  GURL gurl(url);
  if (!gurl.is_valid())
    gurl = extension->GetResourceURL(url);

  scoped_refptr<content::SiteInstance> site_instance =
      opener_render_frame_host ? opener_render_frame_host->GetSiteInstance()
                               : nullptr;
  if (!site_instance ||
      site_instance->GetSiteURL().GetOrigin() != gurl.GetOrigin()) {
    site_instance = content::SiteInstance::CreateForURL(profile, gurl);
  }
  content::WebContents::CreateParams create_params(profile,
                                                   std::move(site_instance));
  if (opener_render_frame_host) {
    create_params.opener_render_process_id =
        opener_render_frame_host->GetProcess()->GetID();
    create_params.opener_render_frame_id =
        opener_render_frame_host->GetRoutingID();
  }
  web_contents_ = content::WebContents::Create(create_params);
  web_contents_->SetDelegate(this);
  content::OpenURLParams params(gurl, content::Referrer(),
                                WindowOpenDisposition::SINGLETON_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  web_contents_->OpenURL(params);

  native_window_ = CreateNativeWindow(this, bounds, web_contents_.get());
}

void ImeWindow::Show() {
  native_window_->Show();
}

void ImeWindow::Hide() {
  native_window_->Hide();
}

void ImeWindow::Close() {
  web_contents_.reset();
  native_window_->Close();
}

void ImeWindow::SetBounds(const gfx::Rect& bounds) {
  native_window_->SetBounds(bounds);
}

void ImeWindow::FollowCursor(const gfx::Rect& cursor_bounds) {
  if (mode_ != FOLLOW_CURSOR)
    return;

  gfx::Rect screen_bounds =
      display::FindDisplayNearestPoint(
          display::Screen::GetScreen()->GetAllDisplays(),
          gfx::Point(cursor_bounds.x(), cursor_bounds.y()))
          ->bounds();
  gfx::Rect window_bounds = native_window_->GetBounds();
  int screen_width = screen_bounds.x() + screen_bounds.width();
  int screen_height = screen_bounds.y() + screen_bounds.height();
  int width = window_bounds.width();
  int height = window_bounds.height();
  // By default, aligns the left of the window client area to the left of the
  // cursor, and aligns the top of the window to the bottom of the cursor.
  // If the right of the window would go beyond the screen bounds, aligns the
  // right of the window to the screen bounds.
  // If the bottom of the window would go beyond the screen bounds, aligns the
  // bottom of the window to the cursor top.
  int x = cursor_bounds.x() - kFollowCursorOffset;
  int y = cursor_bounds.y() + cursor_bounds.height() + kFollowCursorMargin;
  if (width < screen_width && x + width > screen_width)
    x = screen_width - width;
  if (height < screen_height && y + height > screen_height)
    y = cursor_bounds.y() - height - kFollowCursorMargin;
  window_bounds.set_x(x);
  window_bounds.set_y(y);
  SetBounds(window_bounds);
}

int ImeWindow::GetFrameId() const {
  return web_contents_->GetMainFrame()->GetRoutingID();
}

void ImeWindow::OnWindowDestroyed() {
  for (ImeWindowObserver& observer : observers_)
    observer.OnWindowDestroyed(this);
  native_window_ = nullptr;
  delete this;
}

void ImeWindow::AddObserver(ImeWindowObserver* observer) {
  observers_.AddObserver(observer);
}

void ImeWindow::RemoveObserver(ImeWindowObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ImeWindow::OnExtensionIconImageChanged(extensions::IconImage* image) {
  if (native_window_)
    native_window_->UpdateWindowIcon();
}

ImeWindow::~ImeWindow() {}

void ImeWindow::Observe(int type,
                        const content::NotificationSource& source,
                        const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);
  Close();
}

content::WebContents* ImeWindow::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  source->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(params));
  return source;
}

bool ImeWindow::CanDragEnter(content::WebContents* source,
                             const content::DropData& data,
                             blink::WebDragOperationsMask operations_allowed) {
  return false;
}

void ImeWindow::CloseContents(content::WebContents* source) {
  Close();
}

void ImeWindow::SetContentsBounds(content::WebContents* source,
                                  const gfx::Rect& bounds) {
  if (!native_window_)
    return;

  if (mode_ == NORMAL) {
    native_window_->SetBounds(bounds);
    return;
  }

  // Follow-cursor window needs to remain the x/y and only allow JS to
  // change the size.
  gfx::Rect native_bounds = native_window_->GetBounds();
  native_bounds.set_width(bounds.width());
  native_bounds.set_height(bounds.height());
  native_window_->SetBounds(native_bounds);
}

}  // namespace ui
