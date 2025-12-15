// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_TEST_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_TEST_UTILS_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/test_omnibox_edit_model.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"
#include "chrome/browser/ui/webui/searchbox/lens_searchbox_client.h"
#include "components/lens/tab_contextualization_controller.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/composebox/composebox_query.mojom.h"
#include "realbox_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockTabContextualizationController
    : public lens::TabContextualizationController {
 public:
  explicit MockTabContextualizationController(
      tabs::TabInterface* tab_interface);
  ~MockTabContextualizationController() override;

  MOCK_METHOD(void,
              GetPageContext,
              (GetPageContextCallback callback),
              (override));
  MOCK_METHOD(void,
              CaptureScreenshot,
              (std::optional<lens::ImageEncodingOptions> image_options,
               CaptureScreenshotCallback callback),
              (override));
};

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
  MOCK_METHOD(void, SetKeywordSelected, (bool is_keyword_selected), (override));
  MOCK_METHOD(void, OnShow, ());
  MOCK_METHOD(void, SetInputText, (const std::string& input_text));
  MOCK_METHOD(void,
              SetThumbnail,
              (const std::string& thumbnail_url, bool is_deletable));
  MOCK_METHOD(void,
              OnContextualInputStatusChanged,
              (const base::UnguessableToken&,
               composebox_query::mojom::FileUploadStatus,
               std::optional<composebox_query::mojom::FileUploadErrorType>));
  MOCK_METHOD(void, OnTabStripChanged, ());
  MOCK_METHOD(void,
              AddFileContext,
              (const base::UnguessableToken&,
               searchbox::mojom::SelectedFileInfoPtr));
  MOCK_METHOD(void,
              UpdateAutoSuggestedTabContext,
              (searchbox::mojom::TabInfoPtr));
  MOCK_METHOD(void, UpdateLensSearchEligibility, (bool eligible), (override));
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
  explicit MockOmniboxEditModel(OmniboxController* omnibox_controller);
  ~MockOmniboxEditModel() override;
  MockOmniboxEditModel(const MockOmniboxEditModel&) = delete;
  MockOmniboxEditModel& operator=(const MockOmniboxEditModel&) = delete;

  // OmniboxEditModel:
  MOCK_METHOD(void, SetUserText, (const std::u16string&), (override));
  MOCK_METHOD(void, OpenAiMode, (bool, bool), (override));
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
  MOCK_METHOD(lens::proto::LensOverlaySuggestInputs,
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
