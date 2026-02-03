// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_REMOTE_SUGGESTIONS_SERVICE_SIMPLE_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_REMOTE_SUGGESTIONS_SERVICE_SIMPLE_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "third_party/omnibox_proto/page_vertical.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"

namespace action_chips {

// An abstract class for objects that offer a simpler interface to
// RemoteSuggestionsService.
class RemoteSuggestionsServiceSimple {
 public:
  virtual ~RemoteSuggestionsServiceSimple();

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(ParseFailureReason)
  enum class ParseFailureReason {
    // Zero value. This should not occur in production.
    kUnknown = 0,
    // The response from the remote is empty.
    kResponseEmpty = 1,
    // The parse phase fails because the response was not a JSON or its format
    // is not in the expected form.
    kMalformedJson = 2,
    // Used after successfully parsing the JSON response.
    // The parse phase fails because some precondition is not met, an unknown
    // enum value is contained, etc.
    kSchemaMismatch = 3,
    kMaxValue = kSchemaMismatch,
  };
  // LINT.ThenChange(//tools/metrics/histograms/enums.xml:ActionChipsParseError)

  struct NetworkError;
  struct ParseError;
  using Error = std::variant<NetworkError, ParseError>;
  using ActionChipSuggestionsResult =
      base::expected<SearchSuggestionParser::SuggestResults, Error>;

  // Makes a call to a suggestions endpoint to retrieve suggestions used for
  // deep dive chips.
  // Args:
  // - title: the title of a tab
  // - url: the url of a tab
  // - callback: the callback run when the remote call is complete.
  virtual std::unique_ptr<network::SimpleURLLoader>
  GetDeepdiveChipSuggestionsForTab(
      const std::u16string_view title,
      const GURL& url,
      base::OnceCallback<void(ActionChipSuggestionsResult&&)> callback) = 0;

  // Makes a call to a suggestions endpoint to retrieve suggestions used by the
  // steady/deep-dive state.
  // Args:
  // - title: the title of a recent tab, if any.
  // - url: the url of a recent tab, if any.
  // - allowed_tools: a list of tool/model pairs that are allowed to be used.
  // - page_vertical: the vertical information of the page to be passed to the
  //   remote endpoint.
  // - callback: the callback run when the remote call is complete.
  virtual std::unique_ptr<network::SimpleURLLoader> GetActionChipSuggestions(
      base::optional_ref<const std::u16string> title,
      base::optional_ref<const GURL> url,
      base::span<const omnibox::ToolMode> allowed_tools,
      base::optional_ref<const omnibox::PageVertical> page_vertical,
      base::OnceCallback<
          void(RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>
          callback) = 0;

  struct NetworkError {
    int net_error = 0;
    int http_response_code = 0;
  };
  struct ParseError {
    ParseFailureReason parse_failure_reason = ParseFailureReason::kUnknown;
  };
};

class RemoteSuggestionsServiceSimpleImpl
    : public RemoteSuggestionsServiceSimple {
 public:
  explicit RemoteSuggestionsServiceSimpleImpl(
      AutocompleteProviderClient* client);
  ~RemoteSuggestionsServiceSimpleImpl() override;

  std::unique_ptr<network::SimpleURLLoader> GetDeepdiveChipSuggestionsForTab(
      const std::u16string_view title,
      const GURL& url,
      base::OnceCallback<
          void(RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>
          callback) override;

  std::unique_ptr<network::SimpleURLLoader> GetActionChipSuggestions(
      base::optional_ref<const std::u16string> title,
      base::optional_ref<const GURL> url,
      base::span<const omnibox::ToolMode> allowed_tools,
      base::optional_ref<const omnibox::PageVertical> page_vertical,
      base::OnceCallback<
          void(RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>
          callback) override;

 private:
  // Processes the response from the remote endpoint and run the callback with
  // the processing result.
  // This is created to be passed to the callback argument of
  // RemoteSuggestionsService.
  void HandleActionChipSuggestionsResponse(
      base::OnceCallback<
          void(RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>
          callback,
      const bool allow_empty_suggestion,
      const network::SimpleURLLoader* source,
      int response_code,
      std::optional<std::string> response_body);

  raw_ptr<AutocompleteProviderClient> client_;
  base::WeakPtrFactory<RemoteSuggestionsServiceSimpleImpl> weak_ptr_factory_{
      this};
};

}  // namespace action_chips

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_REMOTE_SUGGESTIONS_SERVICE_SIMPLE_H_
