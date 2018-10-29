// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_pedal_implementations.h"

#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_pedal.h"
#include "components/strings/grit/components_strings.h"

// A small convenience wrapper for the common implementation pattern below.
class OmniboxPedalCommon : public OmniboxPedal {
 public:
  OmniboxPedalCommon(LabelStrings strings,
                     GURL url,
                     std::initializer_list<const char*> triggers)
      : OmniboxPedal(strings) {
    url_ = url;
    for (const char* trigger : triggers) {
      triggers_.insert(base::ASCIIToUTF16(trigger));
    }
  }
};

// =============================================================================

class OmniboxPedalClearBrowsingData : public OmniboxPedalCommon {
 public:
  OmniboxPedalClearBrowsingData()
      : OmniboxPedalCommon(
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_HINT,
                IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_HINT_SHORT,
                IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_SUGGESTION_CONTENTS),
            GURL("chrome://settings/clearBrowserData"),
            {
                "how to clear browsing data on chrome",
                "how to clear history",
                "how to clear history on google chrome",
                "how to clear history on chrome",
                "how to clear history in google chrome",
                "how to clear google chrome history",
                "how to clear history google chrome",
                "how to clear browsing history in chrome",
                "clear browsing data",
                "clear history",
                "clear browsing data on chrome",
                "clear history on google chrome",
                "clear history google chrome",
                "clear browsing history in chrome",
                "clear cookies chrome",
                "clear chrome history",
                "clear chrome cache",
                "history clear",
                "history clear chrome",
            }) {}
};

// =============================================================================

class OmniboxPedalChangeSearchEngine : public OmniboxPedalCommon {
 public:
  OmniboxPedalChangeSearchEngine()
      : OmniboxPedalCommon(
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CHANGE_SEARCH_ENGINE_HINT,
                IDS_OMNIBOX_PEDAL_CHANGE_SEARCH_ENGINE_HINT_SHORT,
                IDS_OMNIBOX_PEDAL_CHANGE_SEARCH_ENGINE_SUGGESTION_CONTENTS),
            GURL("chrome://settings/searchEngines"),
            {
                "how to change search engine",
                "how to change default search engine",
                "how to change default search engine in chrome",
                "how to change search engine on chrome",
                "how to set google as default search engine on chrome",
                "how to set google as default search engine in chrome",
                "how to make google default search engine",
                "how to change default search engine in google chrome",
                "change search engine", "change google search engine",
                "change chrome searh engine",
                "change default search engine in chrome",
                "change search engine chrome", "change default search chrome",
                "change search chrome", "switch chrome search engine",
                "switch search engine",
            }) {}
};

// =============================================================================

class OmniboxPedalManagePasswords : public OmniboxPedalCommon {
 public:
  OmniboxPedalManagePasswords()
      : OmniboxPedalCommon(
            LabelStrings(
                IDS_OMNIBOX_PEDAL_MANAGE_PASSWORDS_HINT,
                IDS_OMNIBOX_PEDAL_MANAGE_PASSWORDS_HINT_SHORT,
                IDS_OMNIBOX_PEDAL_MANAGE_PASSWORDS_SUGGESTION_CONTENTS),
            GURL("chrome://settings/passwords"),
            {
                "passwords", "find my passwords", "save passwords in chrome",
                "view saved passwords", "delete passwords",
                "find saved passwords", "where does chrome store passwords",
                "how to see passwords in chrome",
            }) {}
};

// =============================================================================

// TODO(orinj): Use better scoping for existing setting, or link to new UI.
class OmniboxPedalChangeHomePage : public OmniboxPedalCommon {
 public:
  OmniboxPedalChangeHomePage()
      : OmniboxPedalCommon(
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CHANGE_HOME_PAGE_HINT,
                IDS_OMNIBOX_PEDAL_CHANGE_HOME_PAGE_HINT_SHORT,
                IDS_OMNIBOX_PEDAL_CHANGE_HOME_PAGE_SUGGESTION_CONTENTS),
            GURL("chrome://settings/?search=show+home+button"),
            {
                "how to change home page", "how to change your home page",
                "how do i change my home page", "change home page google",
                "home page chrome", "change home chrome",
                "change chrome home page", "how to change home page on chrome",
                "how to change home page in chrome", "change chrome home",
            }) {}
};

// =============================================================================

class OmniboxPedalUpdateCreditCard : public OmniboxPedalCommon {
 public:
  OmniboxPedalUpdateCreditCard()
      : OmniboxPedalCommon(
            OmniboxPedal::LabelStrings(
                IDS_OMNIBOX_PEDAL_UPDATE_CREDIT_CARD_HINT,
                IDS_OMNIBOX_PEDAL_UPDATE_CREDIT_CARD_HINT_SHORT,
                IDS_OMNIBOX_PEDAL_UPDATE_CREDIT_CARD_SUGGESTION_CONTENTS),
            GURL("chrome://settings/autofill"),
            {
                "how to save credit card info on chrome",
                "how to remove credit card from google chrome",
                "remove google chrome credit cards",
                "access google chrome credit cards",
                "google chrome credit cards", "chrome credit cards",
                "get to chrome credit cards", "chrome credit saved",
            }) {}
};

// =============================================================================

class OmniboxPedalLaunchIncognito : public OmniboxPedalCommon {
 public:
  OmniboxPedalLaunchIncognito()
      : OmniboxPedalCommon(
            LabelStrings(
                IDS_OMNIBOX_PEDAL_LAUNCH_INCOGNITO_HINT,
                IDS_OMNIBOX_PEDAL_LAUNCH_INCOGNITO_HINT_SHORT,
                IDS_OMNIBOX_PEDAL_LAUNCH_INCOGNITO_SUGGESTION_CONTENTS),
            GURL(),
            {
                "what is incognito", "what's incognito mode",
            }) {}

  void Execute(ExecutionContext& context) const override {
    context.client_.NewIncognitoWindow();
  }
};

// =============================================================================

class OmniboxPedalTranslate : public OmniboxPedalCommon {
 public:
  OmniboxPedalTranslate()
      : OmniboxPedalCommon(
            LabelStrings(IDS_OMNIBOX_PEDAL_TRANSLATE_HINT,
                         IDS_OMNIBOX_PEDAL_TRANSLATE_HINT_SHORT,
                         IDS_OMNIBOX_PEDAL_TRANSLATE_SUGGESTION_CONTENTS),
            GURL(),
            {
                "how to change language in google chrome",
                "change language chrome", "change chrome language",
                "change language in chrome", "switch chrome language",
                "translate language", "translate in chrome",
                "translate on page", "translate language chrome",
            }) {}

  void Execute(ExecutionContext& context) const override {
    context.client_.PromptPageTranslation();
  }
};

// =============================================================================

class OmniboxPedalUpdateChrome : public OmniboxPedalCommon {
 public:
  OmniboxPedalUpdateChrome()
      : OmniboxPedalCommon(
            LabelStrings(IDS_OMNIBOX_PEDAL_UPDATE_CHROME_HINT,
                         IDS_OMNIBOX_PEDAL_UPDATE_CHROME_HINT_SHORT,
                         IDS_OMNIBOX_PEDAL_UPDATE_CHROME_SUGGESTION_CONTENTS),
            GURL(),
            {
                "how to update google chrome", "how to update chrome",
                "how do i update google chrome", "how to update chrome browser",
                "update google chrome", "update chrome",
                "update chrome browser",
            }) {}

  void Execute(ExecutionContext& context) const override {
    context.client_.OpenUpdateChromeDialog();
  }
};

// =============================================================================

std::vector<std::unique_ptr<OmniboxPedal>> GetPedalImplementations() {
  std::vector<std::unique_ptr<OmniboxPedal>> pedals;
  const auto add = [&](OmniboxPedal* pedal) {
    pedals.push_back(std::unique_ptr<OmniboxPedal>(pedal));
  };
  add(new OmniboxPedalClearBrowsingData());
  add(new OmniboxPedalChangeSearchEngine());
  add(new OmniboxPedalManagePasswords());
  add(new OmniboxPedalChangeHomePage());
  add(new OmniboxPedalUpdateCreditCard());
  add(new OmniboxPedalLaunchIncognito());
  add(new OmniboxPedalTranslate());
  add(new OmniboxPedalUpdateChrome());
  return pedals;
}
