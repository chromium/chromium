// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/arc/intent_helper/arc_intent_helper_mojo_ash.h"
#include "chrome/browser/ash/guest_os/guest_os_external_protocol_handler.h"
#include "chrome/browser/chromeos/arc/arc_external_protocol_dialog.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/external_protocol_dialog.h"
#include "chrome/browser/ui/views/web_apps/protocol_handler_picker_coordinator.h"
#include "chrome/browser/web_applications/app_service/publisher_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "url/gurl.h"

using content::WebContents;


namespace {

void OnArcHandled(const GURL& url,
                  const std::optional<url::Origin>& initiating_origin,
                  content::WeakDocumentPtr initiator_document,
                  base::WeakPtr<WebContents> web_contents,
                  bool handled) {
  if (handled) {
    return;
  }

  // If WebContents have been destroyed, do not show any dialog.
  if (!web_contents) {
    return;
  }

  aura::Window* parent_window = web_contents->GetTopLevelNativeWindow();
  // If WebContents has been detached from window tree, do not show any dialog.
  if (!parent_window || !parent_window->GetRootWindow()) {
    return;
  }

  // Display the standard ExternalProtocolDialog if Guest OS has a handler.
  std::optional<guest_os::GuestOsUrlHandler> registration =
      guest_os::GuestOsUrlHandler::GetForUrl(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()), url);
  if (registration) {
    new ExternalProtocolDialog(web_contents.get(), url,
                               base::UTF8ToUTF16(registration->name()),
                               initiating_origin, initiator_document);
  }
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolHandler

// static
void ExternalProtocolHandler::RunExternalProtocolDialog(
    const GURL& url,
    WebContents* web_contents,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    bool is_in_fenced_frame_tree,
    const std::optional<url::Origin>& initiating_origin,
    content::WeakDocumentPtr initiator_document,
    const std::u16string& program_name) {
  // Don't launch anything from Shimless RMA app.
  if (ash::IsShimlessRmaAppBrowserContext(web_contents->GetBrowserContext())) {
    return;
  }

  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (std::vector<std::string> app_ids =
          web_app::GetWebAppIdsForProtocolUrl(profile, url);
      !app_ids.empty()) {
    web_app::LaunchProtocolUrlInPreferredApp(web_contents, url, app_ids,
                                             initiating_origin);
    return;
  }

  // Check if ARC version of the dialog is available and run ARC
  // version when possible.
  arc::RunArcExternalProtocolDialog(
      url, initiating_origin, web_contents->GetWeakPtr(), page_transition,
      has_user_gesture, is_in_fenced_frame_tree,
      std::make_unique<arc::ArcIntentHelperMojoAsh>(),
      base::BindOnce(&OnArcHandled, url, initiating_origin,
                     std::move(initiator_document),
                     web_contents->GetWeakPtr()));
}
