// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_extension_host.h"

#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/common/extension.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace chromecast {

CastExtensionHost::CastExtensionHost(content::BrowserContext* browser_context,
                                     CastWebView::Delegate* delegate,
                                     const extensions::Extension* extension,
                                     const GURL& initial_url,
                                     content::SiteInstance* site_instance,
                                     extensions::ViewType host_type)
    : extensions::ExtensionHost(extension,
                                site_instance,
                                initial_url,
                                host_type),
      browser_context_(browser_context),
      delegate_(delegate) {
  DCHECK(browser_context_);
  DCHECK(delegate_);
}

CastExtensionHost::~CastExtensionHost() {}

bool CastExtensionHost::IsBackgroundPage() const {
  return false;
}

void CastExtensionHost::OnDidStopFirstLoad() {}

void CastExtensionHost::LoadInitialURL() {
  if (!extensions::ExtensionSystem::Get(browser_context_)
           ->runtime_data()
           ->IsBackgroundPageReady(extension())) {
    registrar_.Add(this,
                   extensions::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
                   content::Source<extensions::Extension>(extension()));
    return;
  }

  extensions::ExtensionHost::LoadInitialURL();
}

void CastExtensionHost::ActivateContents(content::WebContents* contents) {
  DCHECK_EQ(contents, host_contents());
  contents->GetRenderViewHost()->GetWidget()->Focus();
}

void CastExtensionHost::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
#if defined(USE_AURA)
  // Resize window
  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();
  aura::Window* content_window = host_contents()->GetNativeView();
  content_window->SetBounds(
      gfx::Rect(display_size.width(), display_size.height()));
#endif
}

bool CastExtensionHost::DidAddMessageToConsole(
    content::WebContents* source,
    int32_t level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  return delegate_->OnAddMessageToConsoleReceived(source, level, message,
                                                  line_no, source_id);
}

void CastExtensionHost::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK_EQ(type, extensions::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY);
  DCHECK(extensions::ExtensionSystem::Get(browser_context())
             ->runtime_data()
             ->IsBackgroundPageReady(extension()));
  LoadInitialURL();
}

}  // namespace chromecast
