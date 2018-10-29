// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/omnibox_page_handler.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_database.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/search_engines/template_url.h"
#include "content/public/browser/web_ui.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

using bookmarks::BookmarkModel;

namespace mojo {

template <>
struct TypeConverter<std::vector<mojom::AutocompleteAdditionalInfoPtr>,
                     AutocompleteMatch::AdditionalInfo> {
  static std::vector<mojom::AutocompleteAdditionalInfoPtr> Convert(
      const AutocompleteMatch::AdditionalInfo& input) {
    std::vector<mojom::AutocompleteAdditionalInfoPtr> array(input.size());
    size_t index = 0;
    for (auto i = input.begin(); i != input.end(); ++i, index++) {
      mojom::AutocompleteAdditionalInfoPtr item(
          mojom::AutocompleteAdditionalInfo::New());
      item->key = i->first;
      item->value = i->second;
      array[index] = std::move(item);
    }
    return array;
  }
};

template <>
struct TypeConverter<mojom::AutocompleteMatchPtr, AutocompleteMatch> {
  static mojom::AutocompleteMatchPtr Convert(const AutocompleteMatch& input) {
    mojom::AutocompleteMatchPtr result(mojom::AutocompleteMatch::New());
    if (input.provider != NULL) {
      result->provider_name = std::string(input.provider->GetName());
      result->provider_done = input.provider->done();
    }
    result->relevance = input.relevance;
    result->deletable = input.deletable;
    result->fill_into_edit = base::UTF16ToUTF8(input.fill_into_edit);
    result->inline_autocompletion =
        base::UTF16ToUTF8(input.inline_autocompletion);
    result->destination_url = input.destination_url.spec();
    result->contents = base::UTF16ToUTF8(input.contents);
    // At this time, we're not bothering to send along the long vector that
    // represent contents classification.  i.e., for each character, what
    // type of text it is.
    result->description = base::UTF16ToUTF8(input.description);
    // At this time, we're not bothering to send along the long vector that
    // represents description classification.  i.e., for each character, what
    // type of text it is.
    result->transition = input.transition;
    result->allowed_to_be_default_match = input.allowed_to_be_default_match;
    result->type = AutocompleteMatchType::ToString(input.type);
    result->has_tab_match = input.has_tab_match;
    if (input.associated_keyword.get() != NULL) {
      result->associated_keyword =
          base::UTF16ToUTF8(input.associated_keyword->keyword);
    }
    result->keyword = base::UTF16ToUTF8(input.keyword);
    result->duplicates = static_cast<int32_t>(input.duplicate_matches.size());
    result->from_previous = input.from_previous;

    result->additional_info =
        mojo::ConvertTo<std::vector<mojom::AutocompleteAdditionalInfoPtr>>(
            input.additional_info);
    return result;
  }
};

template <>
struct TypeConverter<mojom::AutocompleteResultsForProviderPtr,
                     scoped_refptr<AutocompleteProvider>> {
  static mojom::AutocompleteResultsForProviderPtr Convert(
      const scoped_refptr<AutocompleteProvider>& input) {
    mojom::AutocompleteResultsForProviderPtr result(
        mojom::AutocompleteResultsForProvider::New());
    result->provider_name = input->GetName();
    result->results = mojo::ConvertTo<std::vector<mojom::AutocompleteMatchPtr>>(
        input->matches());
    return result;
  }
};

}  // namespace mojo

OmniboxPageHandler::OmniboxPageHandler(
    Profile* profile,
    mojo::InterfaceRequest<mojom::OmniboxPageHandler> request)
    : profile_(profile), binding_(this, std::move(request)) {
  ResetController();
}

OmniboxPageHandler::~OmniboxPageHandler() {}

void OmniboxPageHandler::OnResultChanged(bool default_match_changed) {
  mojom::OmniboxResultPtr result(mojom::OmniboxResult::New());
  result->done = controller_->done();
  result->time_since_omnibox_started_ms =
      (base::Time::Now() - time_omnibox_started_).InMilliseconds();
  const base::string16 host =
      input_.text().substr(input_.parts().host.begin, input_.parts().host.len);
  result->host = base::UTF16ToUTF8(host);
  result->type = AutocompleteInput::TypeToString(input_.type());
  bool is_typed_host;
  if (!LookupIsTypedHost(host, &is_typed_host))
    is_typed_host = false;
  result->is_typed_host = is_typed_host;

  {
    // Copy to an ACMatches to make conversion easier. Since this isn't
    // performance critical we don't worry about the cost here.
    ACMatches matches(controller_->result().begin(),
                      controller_->result().end());
    result->combined_results =
        mojo::ConvertTo<std::vector<mojom::AutocompleteMatchPtr>>(matches);
  }
  result->results_by_provider =
      mojo::ConvertTo<std::vector<mojom::AutocompleteResultsForProviderPtr>>(
          controller_->providers());

  // Fill AutocompleteMatch::starred.
  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile_);
  if (bookmark_model) {
    for (size_t i = 0; i < result->combined_results.size(); ++i) {
      result->combined_results[i]->starred = bookmark_model->IsBookmarked(
          GURL(result->combined_results[i]->destination_url));
    }
    for (size_t i = 0; i < result->results_by_provider.size(); ++i) {
      const mojom::AutocompleteResultsForProvider& result_by_provider =
          *result->results_by_provider[i];
      for (size_t j = 0; j < result_by_provider.results.size(); ++j) {
        result_by_provider.results[j]->starred = bookmark_model->IsBookmarked(
            GURL(result_by_provider.results[j]->destination_url));
      }
    }
  }

  page_->HandleNewAutocompleteResult(std::move(result));
}

bool OmniboxPageHandler::LookupIsTypedHost(const base::string16& host,
                                           bool* is_typed_host) const {
  history::HistoryService* const history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!history_service)
    return false;
  history::URLDatabase* url_db = history_service->InMemoryDatabase();
  if (!url_db)
    return false;
  *is_typed_host =
      url_db->IsTypedHost(base::UTF16ToUTF8(host), /*scheme=*/nullptr);
  return true;
}

void OmniboxPageHandler::SetClientPage(mojom::OmniboxPagePtr page) {
  page_ = std::move(page);
}

void OmniboxPageHandler::StartOmniboxQuery(const std::string& input_string,
                                           int32_t cursor_position,
                                           bool prevent_inline_autocomplete,
                                           bool prefer_keyword,
                                           int32_t page_classification) {
  // Reset the controller.  If we don't do this, then the
  // AutocompleteController might inappropriately set its |minimal_changes|
  // variable (or something else) and some providers will short-circuit
  // important logic and return stale results.  In short, we want the
  // actual results to not depend on the state of the previous request.
  ResetController();
  time_omnibox_started_ = base::Time::Now();
  input_ = AutocompleteInput(
      base::UTF8ToUTF16(input_string), cursor_position,
      static_cast<metrics::OmniboxEventProto::PageClassification>(
          page_classification),
      ChromeAutocompleteSchemeClassifier(profile_));
  input_.set_prevent_inline_autocomplete(prevent_inline_autocomplete);
  input_.set_prefer_keyword(prefer_keyword);
  controller_->Start(input_);
}

void OmniboxPageHandler::ResetController() {
  controller_.reset(new AutocompleteController(
      std::make_unique<ChromeAutocompleteProviderClient>(profile_), this,
      AutocompleteClassifier::DefaultOmniboxProviders()));
}
