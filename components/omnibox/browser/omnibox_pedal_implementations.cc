// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_pedal_implementations.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/buildflags.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_pedal.h"
#include "components/strings/grit/components_strings.h"

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#endif

// =============================================================================

OmniboxPedalClearBrowsingData::OmniboxPedalClearBrowsingData()
    : OmniboxPedal(
          LabelStrings(
              IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_HINT,
              IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_HINT_SHORT,
              IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_SUGGESTION_CONTENTS),
          GURL("chrome://settings/clearBrowserData")) {}

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
const gfx::VectorIcon& OmniboxPedalClearBrowsingData::GetVectorIcon() const {
  return omnibox::kAnswerWhenIsIcon;
}
#endif

// =============================================================================

class OmniboxPedalChangeSearchEngine : public OmniboxPedal {
 public:
  OmniboxPedalChangeSearchEngine()
      : OmniboxPedal(
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CHANGE_SEARCH_ENGINE_HINT,
                IDS_OMNIBOX_PEDAL_CHANGE_SEARCH_ENGINE_HINT_SHORT,
                IDS_OMNIBOX_PEDAL_CHANGE_SEARCH_ENGINE_SUGGESTION_CONTENTS),
            GURL("chrome://settings/searchEngines")) {}
};

// =============================================================================

class OmniboxPedalManagePasswords : public OmniboxPedal {
 public:
  OmniboxPedalManagePasswords()
      : OmniboxPedal(
            LabelStrings(
                IDS_OMNIBOX_PEDAL_MANAGE_PASSWORDS_HINT,
                IDS_OMNIBOX_PEDAL_MANAGE_PASSWORDS_HINT_SHORT,
                IDS_OMNIBOX_PEDAL_MANAGE_PASSWORDS_SUGGESTION_CONTENTS),
            GURL("chrome://settings/passwords")) {}
};

// =============================================================================

// TODO(orinj): Use better scoping for existing setting, or link to new UI.
class OmniboxPedalChangeHomePage : public OmniboxPedal {
 public:
  OmniboxPedalChangeHomePage()
      : OmniboxPedal(
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CHANGE_HOME_PAGE_HINT,
                IDS_OMNIBOX_PEDAL_CHANGE_HOME_PAGE_HINT_SHORT,
                IDS_OMNIBOX_PEDAL_CHANGE_HOME_PAGE_SUGGESTION_CONTENTS),
            GURL("chrome://settings/?search=show+home+button")) {}
};

// =============================================================================

class OmniboxPedalUpdateCreditCard : public OmniboxPedal {
 public:
  OmniboxPedalUpdateCreditCard()
      : OmniboxPedal(
            OmniboxPedal::LabelStrings(
                IDS_OMNIBOX_PEDAL_UPDATE_CREDIT_CARD_HINT,
                IDS_OMNIBOX_PEDAL_UPDATE_CREDIT_CARD_HINT_SHORT,
                IDS_OMNIBOX_PEDAL_UPDATE_CREDIT_CARD_SUGGESTION_CONTENTS),
            GURL("chrome://settings/autofill")) {}
};

// =============================================================================

class OmniboxPedalLaunchIncognito : public OmniboxPedal {
 public:
  OmniboxPedalLaunchIncognito()
      : OmniboxPedal(
            LabelStrings(
                IDS_OMNIBOX_PEDAL_LAUNCH_INCOGNITO_HINT,
                IDS_OMNIBOX_PEDAL_LAUNCH_INCOGNITO_HINT_SHORT,
                IDS_OMNIBOX_PEDAL_LAUNCH_INCOGNITO_SUGGESTION_CONTENTS),
            // Fake URL to distinguish matches.
            GURL("chrome://newtab?incognito=true")) {}

  void Execute(ExecutionContext& context) const override {
    context.client_.NewIncognitoWindow();
  }
};

// =============================================================================

class OmniboxPedalTranslate : public OmniboxPedal {
 public:
  OmniboxPedalTranslate()
      : OmniboxPedal(
            LabelStrings(IDS_OMNIBOX_PEDAL_TRANSLATE_HINT,
                         IDS_OMNIBOX_PEDAL_TRANSLATE_HINT_SHORT,
                         IDS_OMNIBOX_PEDAL_TRANSLATE_SUGGESTION_CONTENTS),
            // Fake URL to distinguish matches.
            GURL("chrome://translate/pedals")) {}

  void Execute(ExecutionContext& context) const override {
    context.client_.PromptPageTranslation();
  }
};

// =============================================================================

OmniboxPedalUpdateChrome::OmniboxPedalUpdateChrome()
    : OmniboxPedal(
          LabelStrings(IDS_OMNIBOX_PEDAL_UPDATE_CHROME_HINT,
                       IDS_OMNIBOX_PEDAL_UPDATE_CHROME_HINT_SHORT,
                       IDS_OMNIBOX_PEDAL_UPDATE_CHROME_SUGGESTION_CONTENTS),
          // Fake URL to distinguish matches.
          GURL("chrome://update/pedals")) {}

void OmniboxPedalUpdateChrome::Execute(ExecutionContext& context) const {
  context.client_.OpenUpdateChromeDialog();
}

bool OmniboxPedalUpdateChrome::IsReadyToTrigger(
    const AutocompleteProviderClient& client) const {
  return client.IsBrowserUpdateAvailable();
}

// =============================================================================

std::unordered_map<OmniboxPedalId, std::unique_ptr<OmniboxPedal>>
GetPedalImplementations() {
  std::unordered_map<OmniboxPedalId, std::unique_ptr<OmniboxPedal>> pedals;
  const auto add = [&](OmniboxPedalId id, OmniboxPedal* pedal) {
    pedals.insert(std::make_pair(id, std::unique_ptr<OmniboxPedal>(pedal)));
  };
  add(OmniboxPedalId::CLEAR_BROWSING_DATA, new OmniboxPedalClearBrowsingData());
  add(OmniboxPedalId::CHANGE_SEARCH_ENGINE,
      new OmniboxPedalChangeSearchEngine());
  add(OmniboxPedalId::MANAGE_PASSWORDS, new OmniboxPedalManagePasswords());
  add(OmniboxPedalId::CHANGE_HOME_PAGE, new OmniboxPedalChangeHomePage());
  add(OmniboxPedalId::UPDATE_CREDIT_CARD, new OmniboxPedalUpdateCreditCard());
  add(OmniboxPedalId::LAUNCH_INCOGNITO, new OmniboxPedalLaunchIncognito());
  add(OmniboxPedalId::TRANSLATE, new OmniboxPedalTranslate());
  add(OmniboxPedalId::UPDATE_CHROME, new OmniboxPedalUpdateChrome());
  return pedals;
}
