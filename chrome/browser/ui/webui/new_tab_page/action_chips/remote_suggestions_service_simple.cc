// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/remote_suggestions_service_simple.h"

#include <memory>
#include <optional>
#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/i18n/char_iterator.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/search_engines/template_url_service.h"
#include "net/base/net_errors.h"
#include "url/url_util.h"

namespace action_chips {

namespace {
constexpr base::TimeDelta kRemoteCallTimeout = base::Milliseconds(1200);
std::u16string TruncateUTF16(const std::u16string_view input,
                             size_t max_length) {
  if (input.empty()) {
    return u"";
  }

  size_t num_chars = 0;
  base::i18n::UTF16CharIterator it(input);
  while (!it.end() && (num_chars < max_length)) {
    it.Advance();
    num_chars++;
  }

  return std::u16string(input.substr(0, it.array_pos()));
}

std::string GenerateTruncatedTitle(const std::u16string_view title) {
  // Maximum length of page title sent to Suggest via `pageTitle` CGI param,
  // expressed as number of Unicode characters (codepoints).
  //
  // NOTE: The actual string value for the CGI param could be longer (in bytes)
  // than this number, due to the way we're encoding the page title before
  // sending it to Suggest. In the worst-case scenario, the total number of
  // bytes sent could be up to 12x the value specified here:
  // `kMaxPageTitleLength` (# of codepoints) x 4 (UTF-8 code-units per codepoint
  // [maximum]) x 3 (due to URL-encoding [worst-case]).
  static constexpr size_t kMaxPageTitleLength = 128;
  std::string truncated =
      base::UTF16ToUTF8(TruncateUTF16(title, kMaxPageTitleLength));
  url::RawCanonOutputT<char> encoded;
  url::EncodeURIComponent(truncated, &encoded);
  return std::string(encoded.view());
}

base::expected<std::optional<std::string>,
               RemoteSuggestionsServiceSimple::Error>
HandleRawRemoteResponse(const network::SimpleURLLoader* source,
                        const int response_code,
                        std::optional<std::string> response_body) {
  DCHECK(source);
  if (source->NetError() != net::OK || response_code != 200) {
    return base::unexpected(RemoteSuggestionsServiceSimple::NetworkError{
        .net_error = source->NetError(), .http_response_code = response_code});
  }

  return std::move(response_body);
}

base::expected<SearchSuggestionParser::SuggestResults,
               RemoteSuggestionsServiceSimple::Error>
ParseZeroSuggestionsResponse(AutocompleteProviderClient* client,
                             std::optional<std::string> response) {
  using enum ::action_chips::RemoteSuggestionsServiceSimple::ParseError::
      ParseErrorType;
  DCHECK(response);

  if (!response || response->empty()) {
    return base::unexpected(RemoteSuggestionsServiceSimple::ParseError{
        .parse_error_type = kResponseEmpty});
  }

  auto response_data = SearchSuggestionParser::DeserializeJsonData(*response);
  if (!response_data.has_value()) {
    return base::unexpected(RemoteSuggestionsServiceSimple::ParseError{
        .parse_error_type = kMalformedJson});
  }

  static const base::NoDestructor<AutocompleteInput> kInputForZeroSuggest([] {
    AutocompleteInput input;
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
    return input;
  }());

  SearchSuggestionParser::Results results;
  if (!SearchSuggestionParser::ParseSuggestResults(
          *response_data, *kInputForZeroSuggest, client->GetSchemeClassifier(),
          /*default_result_relevance=*/
          omnibox::kDefaultRemoteZeroSuggestRelevance,
          /*is_keyword_result=*/false, &results)) {
    return base::unexpected(RemoteSuggestionsServiceSimple::ParseError{
        .parse_error_type = kParseFailure});
  }
  return std::move(results.suggest_results);
}
}  // namespace

RemoteSuggestionsServiceSimple::~RemoteSuggestionsServiceSimple() = default;

RemoteSuggestionsServiceSimpleImpl::RemoteSuggestionsServiceSimpleImpl(
    AutocompleteProviderClient* client)
    : client_(client) {}
RemoteSuggestionsServiceSimpleImpl::~RemoteSuggestionsServiceSimpleImpl() =
    default;

std::unique_ptr<network::SimpleURLLoader>
RemoteSuggestionsServiceSimpleImpl::GetActionChipSuggestionsForTab(
    const std::u16string_view title,
    const GURL& url,
    base::OnceCallback<
        void(RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>
        callback) {
  TemplateURLRef::SearchTermsArgs search_terms_args;
  search_terms_args.current_page_url = url.spec();
  search_terms_args.additional_query_params =
      base::StrCat({"ctxus=1&pageTitle=", GenerateTruncatedTitle(title)});
  search_terms_args.page_classification = metrics::OmniboxEventProto::OTHER;
  search_terms_args.focus_type = metrics::OmniboxFocusType::INTERACTION_FOCUS;

  const TemplateURLService* template_url_service =
      client_->GetTemplateURLService();

  return client_->GetRemoteSuggestionsService(/*create_if_necessary=*/true)
      ->StartZeroPrefixSuggestionsRequest(
          RemoteRequestType::kZeroSuggestPrefetch, client_->IsOffTheRecord(),
          template_url_service->GetDefaultSearchProvider(), search_terms_args,
          template_url_service->search_terms_data(),
          base::BindOnce(&RemoteSuggestionsServiceSimpleImpl::
                             HandleActionChipSuggestionsResponse,
                         this->weak_ptr_factory_.GetWeakPtr(),
                         std::move(callback)),
          kRemoteCallTimeout);
}

void RemoteSuggestionsServiceSimpleImpl::HandleActionChipSuggestionsResponse(
    base::OnceCallback<
        void(RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>
        callback,
    const network::SimpleURLLoader* source,
    int response_code,
    std::optional<std::string> response_body) {
  std::move(callback).Run(
      HandleRawRemoteResponse(source, response_code, std::move(response_body))
          .and_then([this](std::optional<std::string> response) {
            return ParseZeroSuggestionsResponse(this->client_,
                                                std::move(response));
          }));
}

}  // namespace action_chips
