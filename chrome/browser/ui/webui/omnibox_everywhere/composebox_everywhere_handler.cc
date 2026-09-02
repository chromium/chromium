// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_everywhere/composebox_everywhere_handler.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service_factory.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "content/public/browser/web_contents.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace {

class ComposeboxEverywhereClient final : public ComposeboxOmniboxClient {
 public:
  ComposeboxEverywhereClient(Profile* profile,
                             content::WebContents* web_contents,
                             ComposeboxEverywhereHandler* composebox_handler)
      : ComposeboxOmniboxClient(profile, web_contents, composebox_handler) {}

  ~ComposeboxEverywhereClient() override = default;

  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override {
    return metrics::OmniboxEventProto::COMPOSEBOX_EVERYWHERE;
  }
};

}  // namespace

ComposeboxEverywhereHandler::ComposeboxEverywhereHandler(
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler,
    mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
    Profile* profile,
    content::WebContents* web_contents,
    GetSessionHandleCallback get_session_callback,
    ClearSessionHandleCallback clear_session_callback,
    ScreenshareDelegate* screenshare_delegate)
    : ComposeboxHandler(
          std::move(pending_handler),
          std::move(pending_searchbox_handler),
          std::move(pending_searchbox_page),
          profile,
          web_contents,
          std::make_unique<ComposeboxEverywhereClient>(profile,
                                                       web_contents,
                                                       this),
          std::move(get_session_callback),
          std::move(clear_session_callback)),
      service_(OmniboxEverywhereServiceFactory::GetForProfile(profile)) {
  set_screenshare_delegate(screenshare_delegate);
}

ComposeboxEverywhereHandler::~ComposeboxEverywhereHandler() = default;

void ComposeboxEverywhereHandler::OnDriveUploadClicked(
    OnDriveUploadClickedCallback callback) {
  // Notify the service that the Google Drive picker is being opened so it can
  // suppress auto-dismissal of the standalone Omnibox Everywhere widget.
  service_->OnDrivePickerOpened();

  // Since the Omnibox Everywhere widget is a standalone popup without a native
  // embedding browser window, we must dynamically associate the WebContents
  // with the latest active browser window interface. Doing this on each click
  // ensures that even if the previously associated browser tab/window was
  // closed, the flow can still resolve a valid BrowserWindowInterface and
  // successfully reopen the modal picker dialog. If no active browser window
  // exists (e.g. Chrome is running in the background), we pass nullptr and
  // let the DrivePickerHostController handle the top-level dialog.
  ProfileBrowserCollection* profile_collection =
      ProfileBrowserCollection::GetForProfile(profile_);
  CHECK(profile_collection);
  BrowserWindowInterface* active_bwi =
      profile_collection->GetLastActiveBrowser();
  webui::SetBrowserWindowInterface(web_contents_, active_bwi);

  ComposeboxHandler::OnDriveUploadClicked(std::move(callback));
}

void ComposeboxEverywhereHandler::CleanupDrivePicker() {
  ComposeboxHandler::CleanupDrivePicker();
  // Notify the service that the Drive picker has closed (either via success,
  // cancel, or error) so that the widget can regain focus and restore standard
  // auto-dismissal.
  service_->OnDrivePickerClosed();
}

void ComposeboxEverywhereHandler::OpenUrl(
    GURL url,
    const WindowOpenDisposition disposition,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  if (service_) {
    service_->OpenUrl(url, disposition, ui::PAGE_TRANSITION_LINK,
                      std::move(navigation_handle_callback));
  }
}
