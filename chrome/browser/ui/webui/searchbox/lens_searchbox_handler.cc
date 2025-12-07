// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/lens_searchbox_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_omnibox_client.h"
#include "chrome/browser/ui/webui/searchbox/lens_searchbox_client.h"
#include "components/lens/lens_features.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_id.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "url/gurl.h"

namespace {
// Interface that allows the Lens searchbox to interact with its embedder
// (i.e., LensSearchboxController).
class LensOmniboxClient : public SearchboxOmniboxClient {
 public:
  LensOmniboxClient(Profile* profile,
                    content::WebContents* web_contents,
                    LensSearchboxClient* lens_searchbox_client);
  ~LensOmniboxClient() override;

  // OmniboxClient:
  SessionID GetSessionID() const override;
  const GURL& GetURL() const override;
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override;
  std::optional<lens::proto::LensOverlaySuggestInputs>
  GetLensOverlaySuggestInputs() const override;
  void OnThumbnailRemoved() override;
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
      const AutocompleteMatch& alternative_nav_match) override;

 private:
  raw_ptr<LensSearchboxClient> lens_searchbox_client_;
};
LensOmniboxClient::LensOmniboxClient(Profile* profile,
                                     content::WebContents* web_contents,
                                     LensSearchboxClient* lens_searchbox_client)
    : SearchboxOmniboxClient(profile, web_contents),
      lens_searchbox_client_(lens_searchbox_client) {}

LensOmniboxClient::~LensOmniboxClient() = default;

SessionID LensOmniboxClient::GetSessionID() const {
  return lens_searchbox_client_->GetTabId();
}

const GURL& LensOmniboxClient::GetURL() const {
  return lens_searchbox_client_->GetPageURL();
}

std::optional<lens::proto::LensOverlaySuggestInputs>
LensOmniboxClient::GetLensOverlaySuggestInputs() const {
  return lens_searchbox_client_->GetLensSuggestInputs();
}

void LensOmniboxClient::OnThumbnailRemoved() {
  lens_searchbox_client_->OnThumbnailRemoved();
}

metrics::OmniboxEventProto::PageClassification
LensOmniboxClient::GetPageClassification(bool is_prefetch) const {
  return lens_searchbox_client_->GetPageClassification();
}

void LensOmniboxClient::OnAutocompleteAccept(
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
    const AutocompleteMatch& alternative_nav_match) {
  lens_searchbox_client_->OnSuggestionAccepted(
      destination_url, match.type,
      match.subtypes.contains(omnibox::SUBTYPE_ZERO_PREFIX));
}

}  // namespace

LensSearchboxHandler::LensSearchboxHandler(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
    Profile* profile,
    content::WebContents* web_contents,
    LensSearchboxClient* lens_searchbox_client)
    : SearchboxHandler(
          std::move(pending_page_handler),
          profile,
          web_contents,
          std::make_unique<OmniboxController>(
              std::make_unique<LensOmniboxClient>(profile,
                                                  web_contents,
                                                  lens_searchbox_client),
              lens::features::GetLensSearchboxAutocompleteTimeout())),
      lens_searchbox_client_(lens_searchbox_client) {
  autocomplete_controller_observation_.Observe(autocomplete_controller());
}

LensSearchboxHandler::~LensSearchboxHandler() = default;

std::string LensSearchboxHandler::AutocompleteIconToResourceName(
    const gfx::VectorIcon& icon) const {
  // The default icon for contextual suggestions is the subdirectory arrow right
  // icon. For the Lens searchbox, we want to stay consistent with the search
  // loupe instead.
  if (icon.name == omnibox::kSubdirectoryArrowRightIcon.name) {
    return searchbox_internal::kSearchIconResourceName;
  }

  return SearchboxHandler::AutocompleteIconToResourceName(icon);
}

void LensSearchboxHandler::SetPage(
    mojo::PendingRemote<searchbox::mojom::Page> pending_page) {
  SearchboxHandler::SetPage(std::move(pending_page));

  // The client may have text waiting to be sent to the searchbox that it
  // couldn't do earlier since the page binding was not set. So now we let the
  // client know the binding is ready.
  lens_searchbox_client_->OnPageBound();
}

void LensSearchboxHandler::OnFocusChanged(bool focused) {
  SearchboxHandler::OnFocusChanged(focused);
  lens_searchbox_client_->OnFocusChanged(focused);
}

void LensSearchboxHandler::QueryAutocomplete(const std::u16string& input,
                                             bool prevent_inline_autocomplete) {
  lens_searchbox_client_->OnTextModified();

  SearchboxHandler::QueryAutocomplete(input, prevent_inline_autocomplete);
}

void LensSearchboxHandler::SetInputText(const std::string& input_text) {
  page_->SetInputText(input_text);
}

void LensSearchboxHandler::SetThumbnail(const std::string& thumbnail_url,
                                        bool is_deletable) {
  page_->SetThumbnail(thumbnail_url, is_deletable);
}

void LensSearchboxHandler::OnThumbnailRemoved() {
  omnibox_controller()->client()->OnThumbnailRemoved();
}

void LensSearchboxHandler::OnAutocompleteStopTimerTriggered(
    const AutocompleteInput& input) {
  // Only notify the lens controller when autocomplete stop timer is triggered
  // for zero suggest inputs.
  if (input.IsZeroSuggest() && autocomplete_controller()->done()) {
    lens_searchbox_client_->ShowGhostLoaderErrorState();
  }
}

void LensSearchboxHandler::OnResultChanged(AutocompleteController* controller,
                                           bool default_match_changed) {
  SearchboxHandler::OnResultChanged(controller, default_match_changed);
  // Show the ghost loader error state if the result is empty on the last
  // async pass of the autocomplete controller (there will not be anymore
  // updates). controller->done() itself is not a sufficient check since it
  // takes into account kStop update types which occurs when a user unfocuses
  // the searchbox, and the error state should not be shown in this case.
  if (controller->done() &&
      controller->last_update_type() ==
          AutocompleteController::UpdateType::kLastAsyncPass &&
      controller->result().empty()) {
    lens_searchbox_client_->ShowGhostLoaderErrorState();
  }

  if (controller->input().IsZeroSuggest() && !controller->result().empty()) {
    lens_searchbox_client_->OnZeroSuggestShown();
  }
}
