// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_INTERACTIVE_TEST_MIXIN_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_INTERACTIVE_TEST_MIXIN_H_

#include <concepts>
#include <map>
#include <memory>
#include <string>

#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/google/core/common/google_util.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/url_util.h"
#include "ui/base/interaction/element_identifier.h"

template <typename T>
  requires std::derived_from<T, InteractiveBrowserTestApi>
class SearchboxInteractiveTestMixin : public T {
 public:
  template <class... Args>
  explicit SearchboxInteractiveTestMixin(Args&&... args)
      : T(std::forward<Args>(args)...) {}

  ~SearchboxInteractiveTestMixin() override = default;

  SearchboxInteractiveTestMixin(const SearchboxInteractiveTestMixin&) = delete;
  SearchboxInteractiveTestMixin& operator=(
      const SearchboxInteractiveTestMixin&) = delete;

  // Seeds a history result for `query` to guarantee a synchronous autocomplete
  // match.
  auto SeedSearchboxResult(const std::string& query) {
    return T::Do([this, query]() {
      history::HistoryService* history_service =
          HistoryServiceFactory::GetForProfile(
              this->browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS);
      GURL search_url("https://www.google.com/search?q=" + query);
      history_service->AddPage(search_url, base::Time::Now(),
                               history::SOURCE_BROWSED);
      ui_test_utils::WaitForHistoryToLoad(history_service);
    });
  }

  // Waits for the tab to commit a Google search results page for
  // `expected_params`.
  auto WaitForGoogleSearch(
      const ui::ElementIdentifier& tab_id,
      const std::map<std::string, std::string>& expected_params) {
    // Submitting can fire a synchronous same-WebContents OpenURL before
    // WaitForWebContentsNavigation is armed, so poll the committed URL instead
    // of relying on a one-shot navigation event.
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(
        ui::test::PollingElementStateObserver<bool>, kGoogleSearchNavigated);
    return T::Steps(
        T::InAnyContext(T::PollElement(
            kGoogleSearchNavigated, tab_id,
            [expected_params](const ui::TrackedElement* el) {
              const GURL url = el->AsA<TrackedElementWebContents>()
                                   ->owner()
                                   ->web_contents()
                                   ->GetLastCommittedURL();
              if (!google_util::IsGoogleSearchUrl(url)) {
                return false;
              }
              static constexpr base::UnescapeRule::Type kUnescapeRules =
                  base::UnescapeRule::SPACES |
                  base::UnescapeRule::REPLACE_PLUS_WITH_SPACE |
                  base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS |
                  base::UnescapeRule::PATH_SEPARATORS;
              std::map<std::string, std::string> actual_params;
              for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
                std::string key =
                    base::UnescapeURLComponent(it.GetKey(), kUnescapeRules);
                std::string value =
                    base::UnescapeURLComponent(it.GetValue(), kUnescapeRules);
                actual_params[key] = value;
              }
              for (const auto& [key, value] : expected_params) {
                const auto found = actual_params.find(key);
                if (found == actual_params.end() || found->second != value) {
                  return false;
                }
              }
              return true;
            })),
        T::WaitForState(kGoogleSearchNavigated, testing::Optional(true)),
        T::StopObservingState(kGoogleSearchNavigated));
  }

  // Waits for match to render and verifies its text equals `expected_text`.
  auto WaitForMatch(const ui::ElementIdentifier& contents_id,
                    const WebContentsInteractionTestUtil::DeepQuery& match_text,
                    const std::string& expected_text) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kMatchReady);
    WebContentsInteractionTestUtil::StateChange match_ready;
    match_ready.event = kMatchReady;
    match_ready.where = match_text;
    match_ready.test_function = base::StringPrintf(
        "(el) => el && el.textContent.replace(/\\s+/g, ' ').trim() === '%s'",
        expected_text.c_str());

    return T::Steps(
        T::InAnyContext(T::WaitForElementToRender(contents_id, match_text),
                        T::WaitForStateChange(contents_id, match_ready)));
  }

  // Waits for match to render and verifies its text equals
  // "<input_text> - Google Search".
  auto WaitForVerbatimMatch(
      const ui::ElementIdentifier& contents_id,
      const WebContentsInteractionTestUtil::DeepQuery& match_text,
      const std::string& input_text) {
    return WaitForMatch(contents_id, match_text,
                        input_text + " - Google Search");
  }

  // Waits for a JavaScript condition to evaluate to true for the element
  // resolved by `where`.
  auto WaitForJsConditionAt(
      const ui::ElementIdentifier& contents_id,
      const WebContentsInteractionTestUtil::DeepQuery& where,
      const std::string& test_function) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kJsConditionEvent);
    WebContentsInteractionTestUtil::StateChange state_change;
    state_change.event = kJsConditionEvent;
    state_change.where = where;
    state_change.test_function = test_function;

    return T::Steps(
        T::InAnyContext(T::WaitForStateChange(contents_id, state_change)));
  }

  // Triggers a voice search with the final result matching `result`.
  auto TriggerAimVoiceSearch(
      const ui::ElementIdentifier& contents_id,
      const WebContentsInteractionTestUtil::DeepQuery& voice_search_element,
      const std::string& result) {
    return T::Steps(T::InSameContext(T::ExecuteJsAt(
        contents_id, voice_search_element,
        base::StringPrintf("el => el.dispatchEvent(new CustomEvent("
                           "'voice-search-final-result', "
                           "{detail: '%s', bubbles: true, composed: true}))",
                           result.c_str()))));
  }

  void SetUpOnMainThread() override {
    T::SetUpOnMainThread();
    SetUpUrlLoaderInterceptor();
  }

  void TearDownOnMainThread() override {
    TearDownUrlLoaderInterceptor();
    T::TearDownOnMainThread();
  }

 protected:
  void SetUpUrlLoaderInterceptor() {
    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](content::URLLoaderInterceptor::RequestParams* params) {
              if (params->url_request.url.path() != "/complete/search") {
                return false;
              }

              std::string query;
              std::string q_param;
              if (net::GetValueForKeyInQuery(params->url_request.url, "q",
                                             &q_param)) {
                query = base::UnescapeURLComponent(
                    q_param, base::UnescapeRule::REPLACE_PLUS_WITH_SPACE);
              }

              // Return a mock Autocomplete JSON response array.
              // The response must be prefixed with ")]}'\n" to prevent XSSI
              // if xssi=t was passed.
              std::string response_json = base::StringPrintf(
                  R"()]}'\n["%s", ["suggestion-1", "suggestion-2"]])",
                  query.c_str());

              constexpr std::string_view headers =
                  "HTTP/1.1 200 OK\nContent-Type: application/json\n\n";

              content::URLLoaderInterceptor::WriteResponse(
                  headers, response_json, params->client.get());
              return true;
            }));
  }

  void TearDownUrlLoaderInterceptor() { url_loader_interceptor_.reset(); }

 private:
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_INTERACTIVE_TEST_MIXIN_H_
