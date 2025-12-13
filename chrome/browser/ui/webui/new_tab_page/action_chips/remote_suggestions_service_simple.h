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
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/search_suggestion_parser.h"

namespace action_chips {

// An abstract class for objects that offer a simpler interface to
// RemoteSuggestionsService.
class RemoteSuggestionsServiceSimple {
 public:
  virtual ~RemoteSuggestionsServiceSimple();

  struct NetworkError;
  struct ParseError;
  using Error = std::variant<NetworkError, ParseError>;
  using ActionChipSuggestionsResult =
      base::expected<SearchSuggestionParser::SuggestResults, Error>;

  virtual std::unique_ptr<network::SimpleURLLoader>
  GetActionChipSuggestionsForTab(
      const std::u16string_view title,
      const GURL& url,
      base::OnceCallback<void(ActionChipSuggestionsResult&&)>) = 0;

  struct NetworkError {
    int net_error = 0;
    int http_response_code = 0;
  };
  struct ParseError {
    enum class ParseErrorType {
      // Zero value. This should not occur in production.
      kUnknown,
      // The response from the remote is empty.
      kResponseEmpty,
      // The parse phase fails because the response was not a JSON or its format
      // is not in the expected form.
      kMalformedJson,
      // Used after successfully parsing the JSON response.
      // The parse phase fails because some precondition is not met, an unknown
      // enum value is contained, etc.
      kParseFailure,
    };

    ParseErrorType parse_error_type = ParseErrorType::kUnknown;
  };
};

class RemoteSuggestionsServiceSimpleImpl
    : public RemoteSuggestionsServiceSimple {
 public:
  explicit RemoteSuggestionsServiceSimpleImpl(
      AutocompleteProviderClient* client);
  ~RemoteSuggestionsServiceSimpleImpl() override;

  std::unique_ptr<network::SimpleURLLoader> GetActionChipSuggestionsForTab(
      const std::u16string_view title,
      const GURL& url,
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
      const network::SimpleURLLoader* source,
      int response_code,
      std::optional<std::string> response_body);

  raw_ptr<AutocompleteProviderClient> client_;
  base::WeakPtrFactory<RemoteSuggestionsServiceSimpleImpl> weak_ptr_factory_{
      this};
};

}  // namespace action_chips

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_REMOTE_SUGGESTIONS_SERVICE_SIMPLE_H_
