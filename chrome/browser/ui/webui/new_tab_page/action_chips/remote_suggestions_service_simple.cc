// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/remote_suggestions_service_simple.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/i18n/char_iterator.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "net/base/net_errors.h"
#include "third_party/omnibox_proto/page_vertical.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"
#include "url/origin.h"
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
  return url::EncodeUriComponent(truncated);
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
                             const bool allow_empty_suggestion,
                             std::optional<std::string> response) {
  using enum ::action_chips::RemoteSuggestionsServiceSimple::ParseFailureReason;
  DCHECK(response);

  if (!response || response->empty()) {
    return base::unexpected(RemoteSuggestionsServiceSimple::ParseError{
        .parse_failure_reason = kResponseEmpty});
  }

  auto response_data = SearchSuggestionParser::DeserializeJsonData(*response);
  if (!response_data.has_value()) {
    return base::unexpected(RemoteSuggestionsServiceSimple::ParseError{
        .parse_failure_reason = kMalformedJson});
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
          /*is_keyword_result=*/false,
          {.allow_empty_suggestion = allow_empty_suggestion}, &results)) {
    return base::unexpected(RemoteSuggestionsServiceSimple::ParseError{
        .parse_failure_reason = kSchemaMismatch});
  }
  return std::move(results.suggest_results);
}

std::string CreateAdditionalQueryParams(
    base::optional_ref<const std::u16string> title,
    base::span<const omnibox::ToolMode> allowed_tools,
    base::optional_ref<const omnibox::PageVertical> page_vertical) {
  std::vector<std::string> params;

  if (!allowed_tools.empty()) {
    std::vector<std::string> allowed_tools_strings;
    allowed_tools_strings.reserve(allowed_tools.size());
    for (const auto& tool : allowed_tools) {
      allowed_tools_strings.push_back(base::NumberToString(tool));
    }
    params.push_back(
        base::StrCat({"ats=", base::JoinString(allowed_tools_strings, ",")}));
  }

  if (title.has_value()) {
    params.push_back(
        base::StrCat({"pageTitle=", GenerateTruncatedTitle(*title)}));
  }

  if (page_vertical.has_value()) {
    params.push_back(
        base::StrCat({"pageVertical=", base::NumberToString(*page_vertical)}));
  }
  return base::JoinString(params, "&");
}
}  // namespace

RemoteSuggestionsServiceSimple::~RemoteSuggestionsServiceSimple() = default;

RemoteSuggestionsServiceSimpleImpl::RemoteSuggestionsServiceSimpleImpl(
    AutocompleteProviderClient* client)
    : client_(client) {}
RemoteSuggestionsServiceSimpleImpl::~RemoteSuggestionsServiceSimpleImpl() =
    default;

std::unique_ptr<network::SimpleURLLoader>
RemoteSuggestionsServiceSimpleImpl::GetDeepdiveChipSuggestionsForTab(
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
                         std::move(callback), /*allow_empty_suggestion=*/false),
          kRemoteCallTimeout);
}

void RemoteSuggestionsServiceSimpleImpl::HandleActionChipSuggestionsResponse(
    base::OnceCallback<
        void(RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>
        callback,
    const bool allow_empty_suggestion,
    const network::SimpleURLLoader* source,
    int response_code,
    std::optional<std::string> response_body) {
  std::move(callback).Run(
      HandleRawRemoteResponse(source, response_code, std::move(response_body))
          .and_then([this, allow_empty_suggestion](
                        std::optional<std::string> response) {
            return ParseZeroSuggestionsResponse(
                this->client_, allow_empty_suggestion, std::move(response));
          }));
}

std::unique_ptr<network::SimpleURLLoader>
RemoteSuggestionsServiceSimpleImpl::GetActionChipSuggestions(
    base::optional_ref<const std::u16string> title,
    base::optional_ref<const GURL> url,
    base::span<const omnibox::ToolMode> allowed_tools,
    base::optional_ref<const omnibox::PageVertical> page_vertical,
    base::OnceCallback<
        void(RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>
        callback) {
  TemplateURLRef::SearchTermsArgs search_terms_args;
  if (url.has_value()) {
    search_terms_args.current_page_url = url->spec();
  }
  search_terms_args.additional_query_params =
      CreateAdditionalQueryParams(title, allowed_tools, page_vertical);
  search_terms_args.page_classification =
      metrics::OmniboxEventProto::NTP_ZPS_PREFETCH;
  search_terms_args.request_source =
      SearchTermsData::RequestSource::NTP_ACTION_CHIPS;

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
                         std::move(callback), /*allow_empty_suggestion=*/true),
          kRemoteCallTimeout);
}

}  // namespace action_chips
