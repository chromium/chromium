// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/actions/tab_switch_action.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

// This test is used to verify UI of dedicated row with screenshots verification
// from the base class. This cannot be reworked as a unit test, logic in
// VerifyUI is secondary and isn't as important as UI verification from
// screenshots.
class OmniboxSuggestionButtonRowBrowserTest : public DialogBrowserTest {
 public:
  OmniboxSuggestionButtonRowBrowserTest() = default;

  OmniboxSuggestionButtonRowBrowserTest(
      const OmniboxSuggestionButtonRowBrowserTest&) = delete;
  OmniboxSuggestionButtonRowBrowserTest& operator=(
      const OmniboxSuggestionButtonRowBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    OmniboxViewViews* omnibox_view = GetOmniboxViewViews();
    ASSERT_TRUE(omnibox_view);

    // Populate suggestions for the omnibox popup.
    AutocompleteController* autocomplete_controller =
        omnibox_view->controller()->autocomplete_controller();
    autocomplete_controller->Start({});
    AutocompleteResult& results = autocomplete_controller->internal_result_;
    ACMatches matches;
    TermMatches termMatches = {{0, 0, 0}};

    AutocompleteMatch search_match(nullptr, 500, false,
                                   AutocompleteMatchType::HISTORY_URL);
    search_match.allowed_to_be_default_match = true;
    search_match.contents = u"https://footube.com";
    search_match.description = u"The FooTube";
    search_match.contents_class = ClassifyTermMatches(
        termMatches, search_match.contents.size(),
        ACMatchClassification::MATCH | ACMatchClassification::URL,
        ACMatchClassification::URL);
    search_match.keyword = u"match";
    search_match.associated_keyword = std::make_unique<AutocompleteMatch>();

    auto tab_switch_action = base::MakeRefCounted<TabSwitchAction>(GURL());
    AutocompleteMatch switch_to_tab_match(nullptr, 500, false,
                                          AutocompleteMatchType::HISTORY_URL);
    switch_to_tab_match.contents = u"https://foobar.com";
    switch_to_tab_match.description = u"The Foo Of All Bars";
    switch_to_tab_match.contents_class = ClassifyTermMatches(
        termMatches, switch_to_tab_match.contents.size(),
        ACMatchClassification::MATCH | ACMatchClassification::URL,
        ACMatchClassification::URL);
    switch_to_tab_match.has_tab_match = true;
    switch_to_tab_match.actions.push_back(tab_switch_action);

    AutocompleteMatch action_match(nullptr, 500, false,
                                   AutocompleteMatchType::SEARCH_SUGGEST);
    action_match.contents = u"delete data";
    action_match.description = u"Search";
    action_match.description_class = ClassifyTermMatches(
        termMatches, action_match.description.size(),
        ACMatchClassification::MATCH | ACMatchClassification::URL,
        ACMatchClassification::DIM);
    action_ = base::MakeRefCounted<OmniboxPedal>(
        OmniboxPedalId::CLEAR_BROWSING_DATA,
        OmniboxPedal::LabelStrings(
            IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_HINT,
            IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_SUGGESTION_CONTENTS,
            IDS_ACC_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_SUFFIX,
            IDS_ACC_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA),
        GURL());
    action_match.actions.push_back(action_);

    AutocompleteMatch multiple_actions_match(
        nullptr, 500, false, AutocompleteMatchType::HISTORY_URL);
    multiple_actions_match.contents = u"https://foobarzon.com";
    multiple_actions_match.description = u"The FooBarZon";
    multiple_actions_match.contents_class = ClassifyTermMatches(
        termMatches, multiple_actions_match.contents.size(),
        ACMatchClassification::MATCH | ACMatchClassification::URL,
        ACMatchClassification::URL);
    multiple_actions_match.keyword = u"match";
    multiple_actions_match.associated_keyword =
        std::make_unique<AutocompleteMatch>();
    multiple_actions_match.has_tab_match = true;
    multiple_actions_match.actions.push_back(tab_switch_action);

    matches.push_back(search_match);
    matches.push_back(switch_to_tab_match);
    matches.push_back(action_match);
    matches.push_back(multiple_actions_match);
    results.AppendMatches(matches);
    autocomplete_controller->NotifyChanged();

    // The omnibox popup should open with suggestions displayed.
    omnibox_view->model()->OnPopupResultChanged();
    EXPECT_TRUE(omnibox_view->model()->PopupIsOpen());
  }

  bool VerifyUi() override {
    OmniboxPopupView* popup_view =
        GetOmniboxViewViews()->GetPopupViewForTesting();
    OmniboxEditModel* model = GetOmniboxViewViews()->model();

    model->SetPopupSelection(
        OmniboxPopupSelection(0, OmniboxPopupSelection::KEYWORD_MODE));
    if (!VerifyActiveButtonText(popup_view, 0, u"Search")) {
      return false;
    }

    model->SetPopupSelection(
        OmniboxPopupSelection(1, OmniboxPopupSelection::FOCUSED_BUTTON_ACTION));
    if (!VerifyActiveButtonText(popup_view, 1, u"Switch")) {
      return false;
    }

    model->SetPopupSelection(
        OmniboxPopupSelection(2, OmniboxPopupSelection::FOCUSED_BUTTON_ACTION));
    if (!VerifyActiveButtonText(popup_view, 2, u"Delete")) {
      return false;
    }

    model->SetPopupSelection(
        OmniboxPopupSelection(3, OmniboxPopupSelection::KEYWORD_MODE));
    if (!VerifyActiveButtonText(popup_view, 3, u"Search")) {
      return false;
    }

    model->SetPopupSelection(
        OmniboxPopupSelection(3, OmniboxPopupSelection::FOCUSED_BUTTON_ACTION));
    if (!VerifyActiveButtonText(popup_view, 3, u"Switch")) {
      return false;
    }

    return DialogBrowserTest::VerifyUi();
  }

  std::string GetNonDialogName() override {
    return "RoundedOmniboxResultsFrameWindow";
  }

  OmniboxViewViews* GetOmniboxViewViews() {
    LocationBar* location_bar = browser()->window()->GetLocationBar();
    return static_cast<OmniboxViewViews*>(location_bar->GetOmniboxView());
  }

  bool VerifyActiveButtonText(OmniboxPopupView* popup_view,
                              size_t result_index,
                              const std::u16string& text) {
    return popup_view->GetAccessibleButtonTextForResult(result_index)
               .find(text) != std::u16string::npos;
  }

 private:
  scoped_refptr<OmniboxAction> action_;
};

IN_PROC_BROWSER_TEST_F(OmniboxSuggestionButtonRowBrowserTest, InvokeUi) {
  ShowAndVerifyUi();
}
