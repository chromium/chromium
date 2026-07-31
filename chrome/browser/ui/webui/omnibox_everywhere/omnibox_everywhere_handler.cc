// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_everywhere/omnibox_everywhere_handler.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_omnibox_client.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "content/public/browser/web_ui.h"

namespace {

class OmniboxEverywhereClient : public ContextualOmniboxClient {
 public:
  OmniboxEverywhereClient(Profile* profile,
                          content::WebContents* web_contents,
                          OmniboxEverywhereService* service)
      : ContextualOmniboxClient(profile, web_contents), service_(service) {}
  ~OmniboxEverywhereClient() override = default;

  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override {
    return metrics::OmniboxEventProto::OMNIBOX_EVERYWHERE;
  }

  void OnAutocompleteAccept(
      const GURL& destination_url,
      TemplateURLRef::PostContent* post_content,
      WindowOpenDisposition disposition,
      ui::PageTransition transition,
      AutocompleteMatchType::Type match_type,
      base::TimeTicks match_selection_timestamp,
      bool destination_url_entered_without_scheme,
      bool destination_url_entered_with_http_scheme,
      const std::u16string& text,
      const AutocompleteMatch& match,
      const AutocompleteMatch& alternative_nav_match) override {
    service_->OpenUrl(destination_url, disposition, transition);
  }

 private:
  raw_ptr<OmniboxEverywhereService> service_;
};

}  // namespace

OmniboxEverywhereHandler::OmniboxEverywhereHandler(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
    mojo::PendingRemote<searchbox::mojom::Page> pending_page,
    MetricsReporter* metrics_reporter,
    content::WebUI* web_ui,
    OmniboxEverywhereService* service,
    GetSessionHandleCallback get_session_callback)
    : ContextualSearchboxHandler(
          std::move(pending_page_handler),
          std::move(pending_page),
          Profile::FromWebUI(web_ui),
          web_ui->GetWebContents(),
          std::make_unique<OmniboxEverywhereClient>(Profile::FromWebUI(web_ui),
                                                    web_ui->GetWebContents(),
                                                    service),
          std::move(get_session_callback)),
      service_(service) {
  static_cast<ContextualOmniboxClient*>(client())->SetSuggestInputsCallback(
      base::BindRepeating(&OmniboxEverywhereHandler::GetSuggestInputs,
                          base::Unretained(this)));
  autocomplete_controller_observation_.Observe(autocomplete_controller());
}

OmniboxEverywhereHandler::~OmniboxEverywhereHandler() = default;

void OmniboxEverywhereHandler::OnDriveUploadClicked(
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

  ContextualSearchboxHandler::OnDriveUploadClicked(std::move(callback));
}

void OmniboxEverywhereHandler::CleanupDrivePicker() {
  ContextualSearchboxHandler::CleanupDrivePicker();
  // Notify the service that the Drive picker has closed (either via success,
  // cancel, or error) so that the widget can regain focus and restore standard
  // auto-dismissal.
  service_->OnDrivePickerClosed();
}

void OmniboxEverywhereHandler::OpenProfilePicker() {
  if (service_) {
    service_->ShowProfilePicker();
  }
}
