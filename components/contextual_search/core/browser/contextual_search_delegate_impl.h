// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CORE_BROWSER_CONTEXTUAL_SEARCH_DELEGATE_IMPL_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CORE_BROWSER_CONTEXTUAL_SEARCH_DELEGATE_IMPL_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/contextual_search/core/browser/contextual_search_context.h"
#include "components/contextual_search/core/browser/contextual_search_delegate.h"
#include "components/contextual_search/core/browser/resolved_search_term.h"

namespace content {
class WebContents;
}

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

class TemplateURLService;
class ContextualSearchFieldTrial;

// Handles tasks for the ContextualSearchManager including communicating with
// the server. This class has no JNI in order to keep it separable and testable.
class ContextualSearchDelegateImpl final : public ContextualSearchDelegate {
 public:
  // Constructs a delegate that uses the given url_loader_factory and
  // template_url_service for all contextual search requests.
  ContextualSearchDelegateImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      TemplateURLService* template_url_service);

  ContextualSearchDelegateImpl(const ContextualSearchDelegateImpl&) = delete;
  ContextualSearchDelegateImpl& operator=(const ContextualSearchDelegateImpl&) =
      delete;

  ~ContextualSearchDelegateImpl() override;

  // Gathers surrounding text and saves it in the given context. The given
  // callback will be run when the surrounding text becomes available.
  void GatherAndSaveSurroundingText(
      base::WeakPtr<ContextualSearchContext> contextual_search_context,
      content::WebContents* web_contents,
      SurroundingTextCallback callback) override;

  // Starts an asynchronous search term resolution request.
  // The given context may include some content from a web page and must be able
  // to resolve.
  // When the response is available the given callback will be run.
  void StartSearchTermResolutionRequest(
      base::WeakPtr<ContextualSearchContext> contextual_search_context,
      content::WebContents* web_contents,
      SearchTermResolutionCallback callback) override;

 private:
  // Friend our test which allows our private methods to be used in helper
  // functions.  FRIEND_TEST_ALL_PREFIXES just friends individual prefixes.
  // Needed for |ResolveSearchTermFromContext|.
  friend class ContextualSearchDelegateImplTest;
  // TODO(donnd): consider removing the following since the above covers this.
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateImplTest,
                           SurroundingTextHighMaximum);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateImplTest,
                           SurroundingTextLowMaximum);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateImplTest,
                           SurroundingTextNoBeforeText);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateImplTest,
                           SurroundingTextNoAfterText);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateImplTest,
                           ExtractMentionsStartEnd);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateImplTest,
                           SampleSurroundingText);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateImplTest,
                           SampleSurroundingTextNegativeLimit);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateImplTest,
                           SampleSurroundingTextSameStartEnd);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateImplTest,
                           DecodeSearchTermFromJsonResponse);

  // Resolves the search term specified by the current context.
  void ResolveSearchTermFromContext(
      base::WeakPtr<ContextualSearchContext> context,
      SearchTermResolutionCallback callback);

  // Handles the contextual search response included in |response_body|. Calls
  // |callback| with the resulting ResolvedSearchTerm.
  void OnUrlLoadComplete(base::WeakPtr<ContextualSearchContext> context,
                         SearchTermResolutionCallback callback,
                         std::unique_ptr<std::string> response_body);

  // Builds and returns the search term resolution request URL.
  // |context| is used to help build the query.
  std::string BuildRequestUrl(ContextualSearchContext* context);

  void OnTextSurroundingSelectionAvailable(
      base::WeakPtr<ContextualSearchContext> context,
      SurroundingTextCallback callback,
      const std::u16string& surrounding_text,
      uint32_t start_offset,
      uint32_t end_offset);

  // Builds a Resolved Search Term by decoding the given JSON string.
  std::unique_ptr<ResolvedSearchTerm> GetResolvedSearchTermFromJson(
      const ContextualSearchContext& context,
      int response_code,
      const std::string& json_string);

  // Decodes the given json response string and extracts parameters.
  void DecodeSearchTermFromJsonResponse(
      const std::string& response,
      std::string* search_term,
      std::string* display_text,
      std::string* alternate_term,
      std::string* mid,
      std::string* prevent_preload,
      int* mention_start,
      int* mention_end,
      std::string* context_language,
      std::string* thumbnail_url,
      std::string* caption,
      std::string* quick_action_uri,
      QuickActionCategory* quick_action_category,
      std::string* search_url_full,
      std::string* search_url_preload,
      int* coca_card_tag,
      std::string* related_searches_json);

  // Extracts the start and end location from a mentions list, and sets the
  // integers referenced by |start_result| and |end_result|.
  // |mentions_list| must be a list.
  void ExtractMentionsStartEnd(const base::Value::List& mentions_list,
                               int* start_result,
                               int* end_result) const;

  // Generates a subset of the given surrounding_text string, for usage from
  // Java.
  // |surrounding_text| the entire text context that contains the selection.
  // |padding_each_side| the number of characters of padding desired on each
  // side of the selection (negative values treated as 0).
  // |start| the start offset of the selection, updated to reflect the new
  // position
  // of the selection in the function result.
  // |end| the end offset of the selection, updated to reflect the new position
  // of the selection in the function result.
  // |return| the trimmed surrounding text with selection at the
  // updated start/end offsets.
  std::u16string SampleSurroundingText(const std::u16string& surrounding_text,
                                       int padding_each_side,
                                       size_t* start,
                                       size_t* end) const;

  // The current request in progress, or NULL.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Holds the URL loader factory.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Holds the TemplateURLService. Not owned.
  raw_ptr<TemplateURLService, DanglingUntriaged> template_url_service_;

  // The field trial helper instance, always set up by the constructor.
  std::unique_ptr<ContextualSearchFieldTrial> field_trial_;

  base::WeakPtrFactory<ContextualSearchDelegateImpl> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CORE_BROWSER_CONTEXTUAL_SEARCH_DELEGATE_IMPL_H_
