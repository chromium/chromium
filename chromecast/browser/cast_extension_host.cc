// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_extension_host.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
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

CastExtensionHost::CastExtensionHost(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    const GURL& initial_url,
    scoped_refptr<content::SiteInstance> site_instance)
    : extensions::ExtensionHost(extension,
                                site_instance.get(),
                                initial_url,
                                extensions::VIEW_TYPE_EXTENSION_POPUP),
      browser_context_(browser_context) {
  DCHECK(browser_context_);
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
    blink::mojom::ConsoleMessageLevel log_level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  std::string context = "Cast Extension:";
  base::string16 single_line_message;
  // Mult-line message is not friendly to dumpstate redact.
  base::ReplaceChars(message, base::ASCIIToUTF16("\n"),
                     base::ASCIIToUTF16("\\n "), &single_line_message);
  logging::LogMessage("CONSOLE", line_no, ::logging::LOG_INFO).stream()
      << context << " \"" << single_line_message << "\", source: " << source_id
      << " (" << line_no << ")";
  return true;
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

void CastExtensionHost::EnterFullscreenModeForTab(
    content::WebContents* web_contents,
    const GURL& origin,
    const blink::mojom::FullscreenOptions& options) {
  SetFullscreen(web_contents, true);
}
void CastExtensionHost::ExitFullscreenModeForTab(
    content::WebContents* web_contents) {
  SetFullscreen(web_contents, false);
}
bool CastExtensionHost::IsFullscreenForTabOrPending(
    const content::WebContents* web_contents) {
  return is_fullscreen_;
}

void CastExtensionHost::SetFullscreen(content::WebContents* web_contents,
                                      bool value) {
  if (value == is_fullscreen_)
    return;
  is_fullscreen_ = value;
  web_contents->GetRenderViewHost()->GetWidget()->SynchronizeVisualProperties();
}

}  // namespace chromecast
