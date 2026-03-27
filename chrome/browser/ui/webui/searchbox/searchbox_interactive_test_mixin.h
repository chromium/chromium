// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_INTERACTIVE_TEST_MIXIN_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_INTERACTIVE_TEST_MIXIN_H_

#include <concepts>
#include <string>

#include "base/strings/stringprintf.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/history/core/browser/history_service.h"
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

  // Waits for navigation to a Google search results page for `query`.
  auto WaitForGoogleSearch(const ui::ElementIdentifier& tab_id,
                           const std::string& query) {
    return T::Steps(T::InAnyContext(
        T::WaitForWebContentsNavigation(tab_id),
        T::CheckElement(
            tab_id,
            [](ui::TrackedElement* el) {
              return el->AsA<TrackedElementWebContents>()
                  ->owner()
                  ->web_contents()
                  ->GetLastCommittedURL()
                  .spec();
            },
            testing::StartsWith("https://www.google.com/search?q=" + query))));
  }

  // Waits for match to render and verifies its text equals
  // "<input_text> - Google Search".
  auto WaitForVerbatimMatch(
      const ui::ElementIdentifier& contents_id,
      const WebContentsInteractionTestUtil::DeepQuery& match_text,
      const std::string& input_text) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kVerbatimMatchReady);
    WebContentsInteractionTestUtil::StateChange verbatim_match_ready;
    verbatim_match_ready.event = kVerbatimMatchReady;
    verbatim_match_ready.where = match_text;
    verbatim_match_ready.test_function = base::StringPrintf(
        "(el) => el && el.textContent.replace(/\\s+/g, ' ').trim() === "
        "'%s - Google Search'",
        input_text.c_str());

    return T::Steps(T::InAnyContext(
        T::WaitForElementToRender(contents_id, match_text),
        T::WaitForStateChange(contents_id, verbatim_match_ready)));
  }
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_INTERACTIVE_TEST_MIXIN_H_
