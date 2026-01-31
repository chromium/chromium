// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/omnibox_composebox_handler.h"

#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_utils.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "components/lens/lens_url_utils.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"

namespace {

// OmniboxClient for the omnibox popup composebox.
class OmniboxPopupComposeboxClient : public ContextualOmniboxClient {
 public:
  OmniboxPopupComposeboxClient(Profile* profile,
                               content::WebContents* web_contents,
                               ComposeboxHandler* composebox_handler)
      : ContextualOmniboxClient(profile, web_contents),
        composebox_handler_(composebox_handler) {}

  ~OmniboxPopupComposeboxClient() override = default;

  // OmniboxClient:
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override {
    // This is the omnibox controller associated with the web contents. It's
    // client has access to the location bar which can tell us what
    // classification to return (i.e. differentiate between NTP, SRP, Web).
    // The OmniboxPopupWebContentsHelper should already be instantiated by this
    // point.
    auto* main_omnibox_controller =
        OmniboxPopupWebContentsHelper::FromWebContents(web_contents_)
            ->get_omnibox_controller();

    if (!main_omnibox_controller) {
      return metrics::OmniboxEventProto::OTHER_OMNIBOX_COMPOSEBOX;
    }
    return main_omnibox_controller->client()
        ->GetOmniboxComposeboxPageClassification();
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
    const std::map<std::string, std::string>& additional_params =
        lens::GetParametersMapWithoutQuery(destination_url);

    std::string query_text;
    net::GetValueForKeyInQuery(destination_url, "q", &query_text);
    composebox_handler_->SubmitQuery(
        query_text, disposition,
        PageClassificationToAimEntryPoint(
            GetPageClassification(/*is_prefetch=*/false)),
        additional_params);
  }

 private:
  raw_ptr<ComposeboxHandler> composebox_handler_;
};

}  // namespace

OmniboxComposeboxHandler::OmniboxComposeboxHandler(
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
    mojo::PendingRemote<composebox::mojom::Page> pending_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler,
    Profile* profile,
    content::WebContents* web_contents,
    GetSessionHandleCallback get_session_callback)
    : ComposeboxHandler(
          std::move(pending_handler),
          std::move(pending_page),
          std::move(pending_searchbox_handler),
          profile,
          web_contents,
          std::make_unique<OmniboxController>(
              std::make_unique<OmniboxPopupComposeboxClient>(profile,
                                                             web_contents,
                                                             this)),
          std::move(get_session_callback)) {
  // Set the callback for getting suggest inputs from the session.
  // The session is owned by WebUI controller and accessed via callback.
  // It is safe to use Unretained because omnibox client is owned by `this`.
  static_cast<ContextualOmniboxClient*>(omnibox_controller()->client())
      ->SetSuggestInputsCallback(base::BindRepeating(
          &OmniboxComposeboxHandler::GetSuggestInputs, base::Unretained(this)));
}

OmniboxComposeboxHandler::~OmniboxComposeboxHandler() = default;

void OmniboxComposeboxHandler::HandleFileUpload(bool is_image) {}
