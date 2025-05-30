// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_TEST_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_TEST_UTILS_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/searchbox/lens_searchbox_client.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/browser/test_omnibox_edit_model.h"
#include "realbox_handler.h"
#include "searchbox_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;

class MockSearchboxPage : public searchbox::mojom::Page {
 public:
  MockSearchboxPage();
  ~MockSearchboxPage() override;
  mojo::PendingRemote<searchbox::mojom::Page> BindAndGetRemote();
  mojo::Receiver<searchbox::mojom::Page> receiver_{this};

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD(void,
              AutocompleteResultChanged,
              (searchbox::mojom::AutocompleteResultPtr));
  MOCK_METHOD(void,
              UpdateSelection,
              (searchbox::mojom::OmniboxPopupSelectionPtr,
               searchbox::mojom::OmniboxPopupSelectionPtr));
  MOCK_METHOD(void, SetInputText, (const std::string& input_text));
  MOCK_METHOD(void,
              SetThumbnail,
              (const std::string& thumbnail_url, bool is_deletable));
};

class MockAutocompleteController : public AutocompleteController {
 public:
  MockAutocompleteController(
      std::unique_ptr<AutocompleteProviderClient> provider_client,
      int provider_types);
  ~MockAutocompleteController() override;
  MockAutocompleteController(const MockAutocompleteController&) = delete;
  MockAutocompleteController& operator=(const MockAutocompleteController&) =
      delete;

  // AutocompleteController:
  MOCK_METHOD(void, Start, (const AutocompleteInput&), (override));
};

class MockOmniboxEditModel : public OmniboxEditModel {
 public:
  MockOmniboxEditModel(OmniboxController* omnibox_controller,
                       OmniboxView* view);
  ~MockOmniboxEditModel() override;
  MockOmniboxEditModel(const MockOmniboxEditModel&) = delete;
  MockOmniboxEditModel& operator=(const MockOmniboxEditModel&) = delete;

  // OmniboxEditModel:
  MOCK_METHOD(void, SetUserText, (const std::u16string&), (override));
};

class MockLensSearchboxClient : public LensSearchboxClient {
 public:
  MockLensSearchboxClient();
  ~MockLensSearchboxClient() override;
  MockLensSearchboxClient(const MockLensSearchboxClient&) = delete;
  MockLensSearchboxClient& operator=(const MockLensSearchboxClient&) = delete;

  // LensSearchboxClient:
  MOCK_METHOD(const GURL&, GetPageURL, (), (override, const));
  MOCK_METHOD(SessionID, GetTabId, (), (override, const));
  MOCK_METHOD(metrics::OmniboxEventProto::PageClassification,
              GetPageClassification,
              (),
              (override, const));
  MOCK_METHOD(std::string&, GetThumbnail, (), (override));
  MOCK_METHOD(const lens::proto::LensOverlaySuggestInputs&,
              GetLensSuggestInputs,
              (),
              (override, const));
  MOCK_METHOD(void, OnTextModified, (), (override));
  MOCK_METHOD(void, OnThumbnailRemoved, (), (override));
  MOCK_METHOD(void,
              OnSuggestionAccepted,
              (const GURL&, AutocompleteMatchType::Type, bool),
              (override));
  MOCK_METHOD(void, OnFocusChanged, (bool focused), (override));
  MOCK_METHOD(void, OnPageBound, (), (override));
  MOCK_METHOD(void, ShowGhostLoaderErrorState, (), (override));
  MOCK_METHOD(void, OnZeroSuggestShown, (), (override));
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_TEST_UTILS_H_
