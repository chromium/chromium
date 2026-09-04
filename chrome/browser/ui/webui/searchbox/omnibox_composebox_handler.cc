// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/omnibox_composebox_handler.h"

#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_utils.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/pref_names.h"
#include "components/lens/lens_url_utils.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/contextual_search_provider.h"
#include "components/prefs/pref_service.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

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
        OmniboxPopupWebContentsHelper::GetOrCreateForWebContents(web_contents_)
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
        additional_params, /*is_voice_search=*/false);
  }

 private:
  raw_ptr<ComposeboxHandler> composebox_handler_;
};

}  // namespace

void OmniboxComposeboxHandler::ProcessContextAndOpenUrl(
    GURL url,
    const WindowOpenDisposition disposition) {
  // The voice permission dialog dirties the OS focus history, especially in
  // native Windows OS. Explicitly close the Omnibox popup and claim
  // focus for the WebContents to ensure the Omnibox does not reopen in new
  // page.
  if (omnibox_delegate_) {
    OmniboxController* omnibox_controller =
        omnibox_delegate_->GetOmniboxController();
    if (omnibox_controller) {
      omnibox_controller->StopAutocomplete(/*clear_result=*/true);
      if (web_contents_) {
        web_contents_->Focus();
      }
    }
  }

  ComposeboxHandler::ProcessContextAndOpenUrl(url, disposition);
}

OmniboxComposeboxHandler::OmniboxComposeboxHandler(
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler,
    mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
    Profile* profile,
    content::WebContents* web_contents,
    GetSessionHandleCallback get_session_callback,
    ClearSessionHandleCallback clear_session_callback)
    : ComposeboxHandler(
          std::move(pending_handler),
          std::move(pending_searchbox_handler),
          std::move(pending_searchbox_page),
          profile,
          web_contents,
          std::make_unique<OmniboxPopupComposeboxClient>(profile,
                                                         web_contents,
                                                         this),
          std::move(get_session_callback),
          std::move(clear_session_callback)) {
  // Set the callback for getting suggest inputs from the session.
  // The session is owned by WebUI controller and accessed via callback.
  // It is safe to use Unretained because omnibox client is owned by `this`.
  static_cast<ContextualOmniboxClient*>(client())->SetSuggestInputsCallback(
      base::BindRepeating(&OmniboxComposeboxHandler::GetSuggestInputs,
                          base::Unretained(this)));

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      contextual_search::kSearchContentSharingSettings,
      base::BindRepeating(
          &OmniboxComposeboxHandler::OnContentSharingPolicyChanged,
          base::Unretained(this)));
  OnContentSharingPolicyChanged();

  auto* main_omnibox_controller =
      OmniboxPopupWebContentsHelper::GetOrCreateForWebContents(web_contents_)
          ->get_omnibox_controller();
  if (main_omnibox_controller) {
    popup_state_subscription_ =
        main_omnibox_controller->popup_state_manager()
            ->AddPopupStateChangedCallback(base::BindRepeating(
                &OmniboxComposeboxHandler::OnPopupStateChanged,
                base::Unretained(this)));
    // Perform initial check.
    OnPopupStateChanged(
        OmniboxPopupState::kNone,
        main_omnibox_controller->popup_state_manager()->popup_state());
  }
}

OmniboxComposeboxHandler::~OmniboxComposeboxHandler() = default;

void OmniboxComposeboxHandler::HandleFileUpload(bool is_image) {}

void OmniboxComposeboxHandler::OpenLensSearch() {
  auto* main_omnibox_controller =
      OmniboxPopupWebContentsHelper::GetOrCreateForWebContents(web_contents_)
          ->get_omnibox_controller();
  if (main_omnibox_controller) {
    main_omnibox_controller->edit_model()->OpenLensSearch();
  }
}

void OmniboxComposeboxHandler::OnContentSharingPolicyChanged() {
  page()->UpdateContentSharingPolicy(
      contextual_search::ContextualSearchService::IsContextSharingEnabled(
          profile_->GetPrefs()));
}

// TODO(crbug.com/469098088): If this OnPopupStateChanged approach proves
// reliable for composebox, consider using a similar state-based observation
// in WebuiOmniboxHandler.
void OmniboxComposeboxHandler::OnPopupStateChanged(
    OmniboxPopupState old_state,
    OmniboxPopupState new_state) {
  if (new_state == OmniboxPopupState::kAim) {
    auto* main_omnibox_controller =
        OmniboxPopupWebContentsHelper::GetOrCreateForWebContents(web_contents_)
            ->get_omnibox_controller();
    if (main_omnibox_controller) {
      auto* client = main_omnibox_controller->client();
      GURL current_url = client->GetURL();

      // Manually construct the AutocompleteInput with the current page URL.
      // This is necessary because when AI Mode is opened directly (e.g. by
      // clicking the location bar chip), autocomplete might not have run for
      // the new popup state, so we must calculate eligibility ourselves.
      AutocompleteInput input(
          u"", client->GetPageClassification(/*is_prefetch=*/false),
          client->GetSchemeClassifier());
      input.set_current_url(current_url);

      // Update the Lens search eligibility. We must use the main omnibox
      // controller's AutocompleteProviderClient because it is bound to the
      // active tab's WebContents (where the user is browsing), whereas the
      // composebox's own client is bound to the WebUI popup's WebContents.
      UpdateLensSearchEligibility(
          input, main_omnibox_controller->autocomplete_controller()
                     ->autocomplete_provider_client());
    }
  }
}

void OmniboxComposeboxHandler::UpdateLensSearchEligibility(
    const AutocompleteInput& input,
    AutocompleteProviderClient* client) {
  bool eligible =
      ContextualSearchProvider::LensEntrypointEligible(input, client);
  page()->UpdateLensSearchEligibility(eligible);
}
