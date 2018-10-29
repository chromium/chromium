// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/answer_card/answer_card_search_provider.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/search/answer_card/answer_card_result.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/search_engines/template_url_service.h"

namespace app_list {

namespace {

enum class SearchAnswerRequestResult {
  REQUEST_RESULT_ANOTHER_REQUEST_STARTED = 0,
  REQUEST_RESULT_REQUEST_FAILED = 1,
  REQUEST_RESULT_NO_ANSWER = 2,
  REQUEST_RESULT_RECEIVED_ANSWER = 3,
  REQUEST_RESULT_RECEIVED_ANSWER_TOO_LARGE = 4,
  REQUEST_RESULT_MAX = 5
};

void RecordRequestResult(SearchAnswerRequestResult request_result) {
  UMA_HISTOGRAM_ENUMERATION("SearchAnswer.RequestResult", request_result,
                            SearchAnswerRequestResult::REQUEST_RESULT_MAX);
}

}  // namespace

AnswerCardSearchProvider::NavigationContext::NavigationContext() {}

AnswerCardSearchProvider::NavigationContext::~NavigationContext() {}

void AnswerCardSearchProvider::NavigationContext::StartServerRequest(
    const GURL& url) {
  contents->LoadURL(url);
  state = RequestState::NO_RESULT;
}

void AnswerCardSearchProvider::NavigationContext::Clear() {
  result_url.clear();
  result_title.clear();
  state = RequestState::NO_RESULT;
  // We are not clearing |preferred_size| since the |contents| remains
  // unchanged, and |preferred_size| always corresponds to the contents's size.
}

AnswerCardSearchProvider::AnswerCardSearchProvider(
    Profile* profile,
    AppListModelUpdater* model_updater,
    AppListControllerDelegate* list_controller,
    std::unique_ptr<AnswerCardContents> contents0,
    std::unique_ptr<AnswerCardContents> contents1)
    : profile_(profile),
      model_updater_(model_updater),
      list_controller_(list_controller),
      answer_server_url_(app_list_features::AnswerServerUrl()),
      template_url_service_(TemplateURLServiceFactory::GetForProfile(profile)) {
  navigation_contexts_[0].contents = std::move(contents0);
  navigation_contexts_[1].contents = std::move(contents1);
  navigation_contexts_[0].contents->SetDelegate(this);
  navigation_contexts_[1].contents->SetDelegate(this);
}

AnswerCardSearchProvider::~AnswerCardSearchProvider() {
}

void AnswerCardSearchProvider::Start(const base::string16& query) {
  // Reset the state.
  current_request_url_ = GURL();
  server_request_start_time_ = answer_loaded_time_ = base::TimeTicks();

  if (query.empty() || !model_updater_->SearchEngineIsGoogle()) {
    DeleteCurrentResult();
    return;
  }

  // Start a request to the answer server.

  // Lifetime of |prefixed_query| should be longer than the one of
  // |replacements|.
  const std::string prefixed_query(
      "q=" + net::EscapeQueryParamValue(base::UTF16ToUTF8(query), true) +
      app_list_features::AnswerServerQuerySuffix());
  GURL::Replacements replacements;
  replacements.SetQueryStr(prefixed_query);
  current_request_url_ = answer_server_url_.ReplaceComponents(replacements);
  GetNavigationContextForLoading().StartServerRequest(current_request_url_);

  server_request_start_time_ = base::TimeTicks::Now();
}

void AnswerCardSearchProvider::UpdatePreferredSize(
    const AnswerCardContents* source) {
  if (source != GetCurrentNavigationContext().contents.get())
    return;

  // Contents' size changed for the current card. Updating the result to cause
  // relayout.
  UpdateResult();

  if (!answer_loaded_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("SearchAnswer.ResizeAfterLoadTime",
                        base::TimeTicks::Now() - answer_loaded_time_);
  }
}

void AnswerCardSearchProvider::DidFinishNavigation(
    const AnswerCardContents* source,
    const GURL& url,
    bool has_error,
    bool has_answer_card,
    const std::string& result_title,
    const std::string& issued_query) {
  NavigationContext& context_for_loading = GetNavigationContextForLoading();
  DCHECK_EQ(source, context_for_loading.contents.get());

  if (url != current_request_url_) {
    RecordRequestResult(
        SearchAnswerRequestResult::REQUEST_RESULT_ANOTHER_REQUEST_STARTED);
    return;
  }

  if (has_error) {
    RecordRequestResult(
        SearchAnswerRequestResult::REQUEST_RESULT_REQUEST_FAILED);
    // Loading new card has failed. This invalidates the currently shown result.
    DeleteCurrentResult();
    return;
  }

  if (!has_answer_card) {
    RecordRequestResult(SearchAnswerRequestResult::REQUEST_RESULT_NO_ANSWER);
    // No answer card in the server response. This invalidates the currently
    // shown result.
    DeleteCurrentResult();
    return;
  }
  DCHECK(!result_title.empty());
  DCHECK(!issued_query.empty());
  context_for_loading.result_title = result_title;
  context_for_loading.result_url =
      GetResultUrl(base::UTF8ToUTF16(issued_query));
  RecordRequestResult(
      SearchAnswerRequestResult::REQUEST_RESULT_RECEIVED_ANSWER);

  context_for_loading.state = RequestState::HAVE_RESULT_LOADING;
  UMA_HISTOGRAM_TIMES("SearchAnswer.NavigationTime",
                      base::TimeTicks::Now() - server_request_start_time_);
}

void AnswerCardSearchProvider::OnContentsReady(
    const AnswerCardContents* source) {
  NavigationContext& context_for_loading = GetNavigationContextForLoading();
  DCHECK_EQ(source, context_for_loading.contents.get());

  if (context_for_loading.state != RequestState::HAVE_RESULT_LOADING) {
    // This stop-loading event is either for a navigation that was intercepted
    // by another navigation, or for a failed navigation. In both cases, there
    // is nothing we need to do about it.
    return;
  }

  context_for_loading.state = RequestState::HAVE_RESULT_LOADED;

  // Prepare for loading card into the other contents. Loading will start when
  // the user modifies the query string.
  GetCurrentNavigationContext().Clear();
  current_navigation_context_ = 1 - current_navigation_context_;

  // Show the result.
  UpdateResult();

  answer_loaded_time_ = base::TimeTicks::Now();
  UMA_HISTOGRAM_TIMES("SearchAnswer.LoadingTime",
                      answer_loaded_time_ - server_request_start_time_);
}

void AnswerCardSearchProvider::UpdateResult() {
  SearchProvider::Results results;

  const NavigationContext& current_context = GetCurrentNavigationContext();
  if (current_context.state == RequestState::HAVE_RESULT_LOADED) {
    results.reserve(1);

    const GURL stripped_result_url = AutocompleteMatch::GURLToStrippedGURL(
        GURL(current_context.result_url), AutocompleteInput(),
        template_url_service_, base::string16() /* keyword */);

    results.emplace_back(std::make_unique<AnswerCardResult>(
        profile_, list_controller_, current_context.result_url,
        stripped_result_url.spec(),
        base::UTF8ToUTF16(current_context.result_title),
        current_context.contents.get()));
  }
  SwapResults(&results);
}

std::string AnswerCardSearchProvider::GetResultUrl(
    const base::string16& query) const {
  return template_url_service_->GetDefaultSearchProvider()
      ->url_ref()
      .ReplaceSearchTerms(TemplateURLRef::SearchTermsArgs(query),
                          template_url_service_->search_terms_data());
}

void AnswerCardSearchProvider::DeleteCurrentResult() {
  GetCurrentNavigationContext().Clear();
  UpdateResult();
}

AnswerCardSearchProvider::NavigationContext&
AnswerCardSearchProvider::GetCurrentNavigationContext() {
  return navigation_contexts_[current_navigation_context_];
}

AnswerCardSearchProvider::NavigationContext&
AnswerCardSearchProvider::GetNavigationContextForLoading() {
  return navigation_contexts_[1 - current_navigation_context_];
}

}  // namespace app_list
