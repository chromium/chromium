// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/test_lens_search_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/lens_query_flow_router.h"
#include "chrome/browser/ui/lens/test_lens_overlay_controller.h"
#include "chrome/browser/ui/lens/test_lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/test_lens_search_contextualization_controller.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/variations/variations_client.h"

namespace lens {

namespace {

// The fake server session id.
constexpr char kTestServerSessionId[] = "server_session_id";

// The fake search session id.
constexpr char kTestSearchSessionId[] = "search_session_id";

// The fake suggest signals.
constexpr char kTestSuggestSignals[] = "encoded_image_signals";

lens::Text CreateTestText(const std::vector<std::string>& words) {
  lens::Text text;
  text.set_content_language("es");
  // Create a paragraph.
  lens::TextLayout::Paragraph* paragraph =
      text.mutable_text_layout()->add_paragraphs();
  // Create a line.
  lens::TextLayout::Line* line = paragraph->add_lines();

  for (size_t i = 0; i < words.size(); ++i) {
    lens::TextLayout::Word* word = line->add_words();
    word->set_plain_text(words[i]);
    word->set_text_separator(" ");
    word->mutable_geometry()->mutable_bounding_box()->set_center_x(0.1 * i);
    word->mutable_geometry()->mutable_bounding_box()->set_center_y(0.1);
    word->mutable_geometry()->mutable_bounding_box()->set_width(0.1);
    word->mutable_geometry()->mutable_bounding_box()->set_height(0.1);
    word->mutable_geometry()->mutable_bounding_box()->set_coordinate_type(
        lens::NORMALIZED);
  }
  return text;
}

class FakeTabInterfaceForMocking : public tabs::MockTabInterface {
 public:
  FakeTabInterfaceForMocking() = default;
  ~FakeTabInterfaceForMocking() override = default;

  ui::UnownedUserDataHost& GetUnownedUserDataHost() override {
    return unowned_user_data_host_;
  }

  content::WebContents* GetContents() const override { return nullptr; }

 private:
  ui::UnownedUserDataHost unowned_user_data_host_;
};

class MockTabContextualizationController
    : public TabContextualizationController {
 public:
  explicit MockTabContextualizationController(tabs::TabInterface* tab)
      : TabContextualizationController(tab) {}
  ~MockTabContextualizationController() override = default;

  void GetPageContext(GetPageContextCallback callback) override {
    auto data = std::make_unique<lens::ContextualInputData>();
    data->is_page_context_eligible = true;
    std::move(callback).Run(std::move(data));
  }
};

}  // namespace

MockLensSearchController::MockLensSearchController(tabs::TabInterface* tab)
    : LensSearchController(tab) {}

MockLensSearchController::~MockLensSearchController() = default;

std::unique_ptr<LensOverlayController>
TestLensSearchController::CreateLensOverlayController(
    tabs::TabInterface* tab,
    LensSearchController* lens_search_controller,
    PrefService* pref_service,
    ThemeService* theme_service) {
  // Set browser color scheme to light mode for consistency.
  theme_service->SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kLight);

  return std::make_unique<TestLensOverlayController>(
      tab, lens_search_controller, pref_service);
}

std::unique_ptr<lens::LensOverlayQueryController>
TestLensSearchController::CreateLensQueryController(
    lens::LensOverlayFullImageResponseCallback full_image_callback,
    lens::LensOverlayUrlResponseCallback url_callback,
    lens::LensOverlayInteractionResponseCallback interaction_callback,
    lens::LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
    lens::UploadProgressCallback upload_progress_callback,
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager,
    Profile* profile,
    lens::LensOverlayInvocationSource invocation_source,
    bool use_dark_mode,
    lens::LensOverlayGen204Controller* gen204_controller) {
  auto fake_query_controller =
      std::make_unique<lens::TestLensOverlayQueryController>(
          full_image_callback, url_callback, interaction_callback,
          thumbnail_created_callback, upload_progress_callback,
          variations_client, identity_manager, profile, invocation_source,
          use_dark_mode, gen204_controller);

  // Set up the fake responses for the query controller.
  lens::LensOverlayServerClusterInfoResponse cluster_info_response;
  cluster_info_response.set_server_session_id(kTestServerSessionId);
  cluster_info_response.set_search_session_id(kTestSearchSessionId);
  fake_query_controller->set_fake_cluster_info_response(cluster_info_response);

  lens::LensOverlayObjectsResponse objects_response;
  objects_response.mutable_text()->CopyFrom(
      CreateTestText({"This", "is", "test", "text."}));
  objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  objects_response.mutable_cluster_info()->set_search_session_id(
      kTestSearchSessionId);
  fake_query_controller->set_fake_objects_response(objects_response);

  lens::LensOverlayInteractionResponse interaction_response;
  interaction_response.set_encoded_response(kTestSuggestSignals);
  fake_query_controller->set_fake_interaction_response(interaction_response);
  return fake_query_controller;
}

FakeLensQueryFlowRouter::FakeLensQueryFlowRouter(
    LensSearchController* lens_search_controller)
    : LensQueryFlowRouter(lens_search_controller) {
  mock_handle_ =
      std::make_unique<contextual_search::MockContextualSearchSessionHandle>();
  mock_context_controller_ = std::make_unique<
      contextual_search::MockContextualSearchContextController>();
  ON_CALL(*mock_handle_, CreateSearchUrl(::testing::_, ::testing::_))
      .WillByDefault(
          ::testing::WithArg<1>([](base::OnceCallback<void(GURL)> callback) {
            std::move(callback).Run(GURL("http://dummy-search-url.com"));
          }));
  ON_CALL(*mock_handle_, GetController())
      .WillByDefault(::testing::Return(mock_context_controller_.get()));

  mock_file_info_ = std::make_unique<contextual_search::FileInfo>();

  ON_CALL(*mock_handle_, CreateContextToken())
      .WillByDefault(::testing::Return(base::UnguessableToken::Create()));

  ON_CALL(*mock_handle_,
          StartTabContextUploadFlow(::testing::_, ::testing::_, ::testing::_))
      .WillByDefault(
          ::testing::WithArgs<0>([this](const base::UnguessableToken& token) {
            this->OnContextUploadStatusChangedForTesting(
                token, lens::MimeType::kImage,
                contextual_search::ContextUploadStatus::kUploadSuccessful,
                std::nullopt);
          }));

  ON_CALL(*mock_context_controller_, GetFileInfo(::testing::_))
      .WillByDefault(::testing::Return(mock_file_info_.get()));

  fake_tab_interface_ = std::make_unique<FakeTabInterfaceForMocking>();
  mock_tab_context_controller_ =
      std::make_unique<MockTabContextualizationController>(
          fake_tab_interface_.get());
}

FakeLensQueryFlowRouter::~FakeLensQueryFlowRouter() = default;

bool FakeLensQueryFlowRouter::IsActiveTabContextEligible() const {
  return true;
}

TabContextualizationController*
FakeLensQueryFlowRouter::GetTabContextualizationController() const {
  return mock_tab_context_controller_.get();
}

contextual_search::ContextualSearchSessionHandle*
FakeLensQueryFlowRouter::GetContextualSearchSessionHandle() const {
  return mock_handle_.get();
}

std::unique_ptr<lens::LensQueryFlowRouter>
TestLensSearchController::CreateLensQueryFlowRouter() {
  return std::make_unique<FakeLensQueryFlowRouter>(this);
}

std::unique_ptr<lens::LensSearchContextualizationController>
TestLensSearchController::CreateLensSearchContextualizationController() {
  return std::make_unique<lens::TestLensSearchContextualizationController>(
      this);
}

}  // namespace lens
