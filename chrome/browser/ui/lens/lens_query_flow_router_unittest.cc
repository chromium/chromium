// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_query_flow_router.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/lens/test_lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/test_lens_search_contextualization_controller.h"
#include "chrome/browser/ui/lens/test_lens_search_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_url_utils.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/lens/tab_contextualization_controller.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/lens_overlay_image_crop.pb.h"
#include "ui/gfx/skia_util.h"

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;

namespace lens {

namespace {

MATCHER_P(OptionalBitmapEquals,
          expected_bitmap,
          "Compares two bitmaps with the argument being an optional bitmap") {
  return arg.has_value() && gfx::BitmapsAreEqual(expected_bitmap, arg.value());
}

MATCHER_P(BitmapEquals, expected_bitmap, "Compares two bitmaps") {
  return gfx::BitmapsAreEqual(expected_bitmap, arg);
}

MATCHER_P(ContextualInputDataMatches,
          expected,
          "Compares ContextualInputData") {
  // The viewport screenshots are optional so if they both do not have a value,
  // they are considered equal. This prevents a compile error when trying to
  // compare optional SkBitmaps.
  const bool are_bitmaps_equal =
      (!arg->viewport_screenshot.has_value() &&
       !expected.viewport_screenshot.has_value()) ||
      gfx::BitmapsAreEqual(arg->viewport_screenshot.value(),
                           expected.viewport_screenshot.value());

  return arg->page_url == expected.page_url &&
         arg->page_title == expected.page_title &&
         arg->primary_content_type == expected.primary_content_type &&
         arg->pdf_current_page == expected.pdf_current_page &&
         arg->is_page_context_eligible == expected.is_page_context_eligible &&
         are_bitmaps_equal;
}

using CreateSearchUrlRequestInfo = contextual_search::
    ContextualSearchContextController::CreateSearchUrlRequestInfo;

MATCHER_P(CreateSearchUrlRequestInfoMatches,
          expected,
          "Compares CreateSearchUrlRequestInfo") {
  return arg->search_url_type == expected->search_url_type &&
         arg->query_text == expected->query_text &&
         arg->query_start_time == expected->query_start_time &&
         arg->lens_overlay_selection_type ==
             expected->lens_overlay_selection_type &&
         arg->additional_params == expected->additional_params &&
         arg->image_crop.has_value() == expected->image_crop.has_value();
}

MATCHER_P(ImageEncodingOptionsMatches,
          expected,
          "Compares ImageEncodingOptions") {
  if (!arg.has_value()) {
    return false;
  }
  const auto& actual = arg.value();
  return actual.enable_webp_encoding == expected.enable_webp_encoding &&
         actual.max_size == expected.max_size &&
         actual.max_height == expected.max_height &&
         actual.max_width == expected.max_width &&
         actual.compression_quality == expected.compression_quality;
}

class TestLensQueryFlowRouter : public LensQueryFlowRouter {
 public:
  explicit TestLensQueryFlowRouter(
      LensSearchController* lens_search_controller,
      contextual_search::MockContextualSearchContextController*
          mock_context_controller)
      : LensQueryFlowRouter(lens_search_controller) {
    // Create the session handle immediately so that mock calls can be added
    // immediately.
    pending_mock_session_handle_ = std::make_unique<
        contextual_search::MockContextualSearchSessionHandle>();
    raw_mock_session_handle_ = pending_mock_session_handle_.get();
    ON_CALL(*pending_mock_session_handle_, GetController())
        .WillByDefault(Return(mock_context_controller));
    viewport_screenshot_.allocN32Pixels(10, 10);
  }
  ~TestLensQueryFlowRouter() override = default;

  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
  CreateContextualSearchSessionHandle() override {
    CHECK(pending_mock_session_handle_);
    return std::move(pending_mock_session_handle_);
  }

  const SkBitmap& GetViewportScreenshot() const override {
    return viewport_screenshot_;
  }

  contextual_search::MockContextualSearchSessionHandle* mock_session_handle() {
    return raw_mock_session_handle_;
  }

  void ClearMockSessionHandle() { raw_mock_session_handle_ = nullptr; }

 private:
  SkBitmap viewport_screenshot_;
  std::unique_ptr<contextual_search::MockContextualSearchSessionHandle>
      pending_mock_session_handle_;
  // A reference to the raw pointer of the pending mock session handle. This is
  // needed since the LensQueryFlowRouter::StartQueryFlow() calls std::move on
  // the pending handle. Without this, the session handle will be gone and the
  // test will seg fault.
  raw_ptr<contextual_search::MockContextualSearchSessionHandle>
      raw_mock_session_handle_;
};

class MockTabContextualizationController
    : public TabContextualizationController {
 public:
  explicit MockTabContextualizationController(tabs::TabInterface* tab)
      : TabContextualizationController(tab) {}
  ~MockTabContextualizationController() override = default;

  MOCK_METHOD(void,
              GetPageContext,
              (GetPageContextCallback callback),
              (override));
};

class MockContextualTasksUiService
    : public contextual_tasks::ContextualTasksUiService {
 public:
  explicit MockContextualTasksUiService(Profile* profile)
      : ContextualTasksUiService(profile, nullptr, nullptr) {}
  ~MockContextualTasksUiService() override = default;

  MOCK_METHOD(void,
              StartTaskUiInSidePanel,
              (BrowserWindowInterface * browser_window_interface,
               tabs::TabInterface* tab_interface,
               const GURL& url,
               std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
                   session_handle),
              (override));

  MOCK_METHOD(GURL, GetDefaultAiPageUrl, (), (override));
};

std::unique_ptr<KeyedService> CreateMockContextualTasksUiService(
    content::BrowserContext* context) {
  return std::make_unique<MockContextualTasksUiService>(
      Profile::FromBrowserContext(context));
}

}  // namespace

class LensQueryFlowRouterTest : public testing::Test {
 public:
  LensQueryFlowRouterTest() = default;
  ~LensQueryFlowRouterTest() override = default;

  void SetUp() override {
    InitFeatureList();

    profile_ = std::make_unique<TestingProfile>();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), content::SiteInstance::Create(profile_.get()));

    // The Lens search controller calls `GetUnownedUserDataHost` on the tab
    // interface in its constructor, so set up the mock responses before it is
    // created.
    mock_browser_window_interface_ =
        std::make_unique<MockBrowserWindowInterface>();
    ON_CALL(*mock_browser_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
    ON_CALL(mock_tab_interface_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
    ON_CALL(mock_tab_interface_, GetBrowserWindowInterface())
        .WillByDefault(Return(mock_browser_window_interface_.get()));
    ON_CALL(mock_tab_interface_, GetContents())
        .WillByDefault(Return(web_contents_.get()));

    // Create a mock Lens search controller that returns a mock Lens overlay
    // query controller.
    gen204_controller_ = std::make_unique<LensOverlayGen204Controller>();
    mock_query_controller_ = std::make_unique<MockLensOverlayQueryController>(
        gen204_controller_.get());
    mock_lens_search_controller_ =
        std::make_unique<MockLensSearchController>(&mock_tab_interface_);
    contextualization_controller_ =
        std::make_unique<TestLensSearchContextualizationController>(
            mock_lens_search_controller_.get());
  }

  void TearDown() override {
    mock_query_controller_.reset();
    gen204_controller_.reset();
    contextualization_controller_.reset();
    mock_lens_search_controller_.reset();
    mock_browser_window_interface_.reset();
  }

 protected:
  virtual void InitFeatureList() {
    feature_list_.InitWithFeaturesAndParameters(
        {}, {contextual_tasks::kContextualTasks,
             contextual_tasks::kContextualTasksContext});
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  base::test::ScopedFeatureList feature_list_;
  ui::UnownedUserDataHost user_data_host_;
  tabs::MockTabInterface mock_tab_interface_;
  std::unique_ptr<MockBrowserWindowInterface> mock_browser_window_interface_;
  std::unique_ptr<LensSearchContextualizationController>
      contextualization_controller_;
  std::unique_ptr<MockLensOverlayQueryController> mock_query_controller_;
  std::unique_ptr<LensOverlayGen204Controller> gen204_controller_;
  std::unique_ptr<MockLensSearchController> mock_lens_search_controller_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
};

TEST_F(LensQueryFlowRouterTest, StartQueryFlow_RoutesToLensQueryController) {
  // Arrange: Set up and create the router.
  EXPECT_CALL(*mock_lens_search_controller_, lens_overlay_query_controller())
      .WillOnce(Return(mock_query_controller_.get()));
  LensQueryFlowRouter router(mock_lens_search_controller_.get());

  // Arrange: Set up the start query flow parameters.
  SkBitmap screenshot;
  screenshot.allocN32Pixels(10, 10);
  GURL example_url("https://example.com");
  std::string page_title = "Title";
  lens::MimeType primary_content_type = lens::MimeType::kAnnotatedPageContent;
  float ui_scale_factor = 1.0f;
  base::TimeTicks invocation_time = base::TimeTicks::Now();

  // Assert: Create start query flow function call expectation.
  EXPECT_CALL(*mock_query_controller_,
              StartQueryFlow(BitmapEquals(screenshot), example_url,
                             testing::Eq(page_title), testing::IsEmpty(),
                             testing::IsEmpty(), primary_content_type,
                             testing::Eq(std::nullopt), ui_scale_factor,
                             invocation_time));

  // Act: Start query flow.
  router.StartQueryFlow(screenshot, example_url, page_title, {}, {},
                        primary_content_type, std::nullopt, ui_scale_factor,
                        invocation_time);
}

TEST_F(LensQueryFlowRouterTest, SendRegionSearch_RoutesToLensQueryController) {
  // Arrange: Set up and create the router.
  EXPECT_CALL(*mock_lens_search_controller_, lens_overlay_query_controller())
      .WillOnce(Return(mock_query_controller_.get()));
  LensQueryFlowRouter router(mock_lens_search_controller_.get());

  // Arrange: Set up the parameters.
  base::Time query_start_time = base::Time::Now();
  auto region = lens::mojom::CenterRotatedBox::New();
  lens::LensOverlaySelectionType selection_type =
      lens::LensOverlaySelectionType::REGION_SEARCH;
  std::map<std::string, std::string> additional_params;
  SkBitmap region_bytes;
  region_bytes.allocN32Pixels(10, 10);

  // Assert: Create expectation.
  EXPECT_CALL(
      *mock_query_controller_,
      SendRegionSearch(query_start_time, _, selection_type, additional_params,
                       OptionalBitmapEquals(region_bytes)));

  // Act: Call the method.
  router.SendRegionSearch(query_start_time, std::move(region), selection_type,
                          additional_params, region_bytes);
}

TEST_F(LensQueryFlowRouterTest, SendTextOnlyQuery_RoutesToLensQueryController) {
  // Arrange: Set up and create the router.
  EXPECT_CALL(*mock_lens_search_controller_, lens_overlay_query_controller())
      .WillOnce(Return(mock_query_controller_.get()));
  LensQueryFlowRouter router(mock_lens_search_controller_.get());

  // Arrange: Set up the parameters.
  base::Time query_start_time = base::Time::Now();
  std::string query_text = "test query";
  lens::LensOverlaySelectionType selection_type =
      lens::LensOverlaySelectionType::TRANSLATE_CHIP;
  std::map<std::string, std::string> additional_params;

  // Assert: Create expectation.
  EXPECT_CALL(*mock_query_controller_,
              SendTextOnlyQuery(query_start_time, query_text, selection_type,
                                additional_params));

  // Act: Call the method.
  router.SendTextOnlyQuery(query_start_time, query_text, selection_type,
                           additional_params);
}

TEST_F(LensQueryFlowRouterTest, GetSuggestInputs_RoutesToLensQueryController) {
  // Arrange
  EXPECT_CALL(*mock_lens_search_controller_, lens_overlay_query_controller())
      .WillRepeatedly(Return(mock_query_controller_.get()));
  LensQueryFlowRouter router(mock_lens_search_controller_.get());

  lens::proto::LensOverlaySuggestInputs expected_inputs;
  expected_inputs.set_encoded_request_id("test_id");

  EXPECT_CALL(*mock_query_controller_, IsOff()).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_query_controller_, GetLensSuggestInputs())
      .WillRepeatedly(ReturnRef(expected_inputs));

  // Act
  auto result = router.GetSuggestInputs();

  // Assert
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->encoded_request_id(), "test_id");
}

TEST_F(LensQueryFlowRouterTest,
       SetSuggestInputsReadyCallback_RoutesToLensQueryController) {
  // Arrange
  EXPECT_CALL(*mock_lens_search_controller_, lens_overlay_query_controller())
      .WillRepeatedly(Return(mock_query_controller_.get()));
  LensQueryFlowRouter router(mock_lens_search_controller_.get());

  // The router checks if inputs are ready before setting the callback.
  lens::proto::LensOverlaySuggestInputs empty_inputs;
  EXPECT_CALL(*mock_query_controller_, IsOff()).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_query_controller_, GetLensSuggestInputs())
      .WillOnce(ReturnRef(empty_inputs));
  EXPECT_CALL(*mock_query_controller_, SetSuggestInputsReadyCallback(_));

  // Act
  router.SetSuggestInputsReadyCallback(base::DoNothing());
}

TEST_F(LensQueryFlowRouterTest,
       SendContextualTextQuery_RoutesToLensQueryController) {
  // Arrange: Set up and create the router.
  EXPECT_CALL(*mock_lens_search_controller_, lens_overlay_query_controller())
      .WillOnce(Return(mock_query_controller_.get()));
  LensQueryFlowRouter router(mock_lens_search_controller_.get());

  // Arrange: Set up the parameters.
  base::Time query_start_time = base::Time::Now();
  std::string query_text = "test query";
  lens::LensOverlaySelectionType selection_type =
      lens::LensOverlaySelectionType::MULTIMODAL_SUGGEST_TYPEAHEAD;
  std::map<std::string, std::string> additional_params;

  // Assert: Create expectation.
  EXPECT_CALL(*mock_query_controller_,
              SendContextualTextQuery(query_start_time, query_text,
                                      selection_type, additional_params));

  // Act: Call the method.
  router.SendContextualTextQuery(query_start_time, query_text, selection_type,
                                 additional_params);
}

TEST_F(LensQueryFlowRouterTest,
       SendMultimodalRequest_RoutesToLensQueryController) {
  // Arrange: Set up and create the router.
  EXPECT_CALL(*mock_lens_search_controller_, lens_overlay_query_controller())
      .WillOnce(Return(mock_query_controller_.get()));
  LensQueryFlowRouter router(mock_lens_search_controller_.get());

  // Arrange: Set up the parameters.
  base::Time query_start_time = base::Time::Now();
  auto region = lens::mojom::CenterRotatedBox::New();
  std::string query_text = "test query";
  lens::LensOverlaySelectionType selection_type =
      lens::LensOverlaySelectionType::MULTIMODAL_SEARCH;
  std::map<std::string, std::string> additional_params;
  SkBitmap region_bytes;
  region_bytes.allocN32Pixels(10, 10);

  // Assert: Create expectation.
  EXPECT_CALL(*mock_query_controller_,
              SendMultimodalRequest(query_start_time, _, query_text,
                                    selection_type, additional_params,
                                    OptionalBitmapEquals(region_bytes)));

  // Act: Call the method.
  router.SendMultimodalRequest(query_start_time, std::move(region), query_text,
                               selection_type, additional_params, region_bytes);
}

class LensQueryFlowRouterContextualTaskEnabledTest
    : public LensQueryFlowRouterTest {
 protected:
  void InitFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        {
            {contextual_tasks::kContextualTasksContext, {}},
            {contextual_tasks::kContextualTasks, {}},
        },
        {});
  }

  void SetUp() override {
    LensQueryFlowRouterTest::SetUp();
    mock_context_controller_ = std::make_unique<
        contextual_search::MockContextualSearchContextController>();
    contextual_tasks::ContextualTasksUiServiceFactory::GetInstance()
        ->SetTestingFactory(
            profile_.get(),
            base::BindRepeating(&CreateMockContextualTasksUiService));
    mock_tab_contextualization_controller_ =
        std::make_unique<MockTabContextualizationController>(
            &mock_tab_interface_);
  }

  void TearDown() override {
    // Controller must be destroyed before the tab interface and user data host.
    mock_tab_contextualization_controller_.reset();
    mock_context_controller_.reset();
    LensQueryFlowRouterTest::TearDown();
  }

  std::unique_ptr<MockTabContextualizationController>
      mock_tab_contextualization_controller_;
  std::unique_ptr<contextual_search::MockContextualSearchContextController>
      mock_context_controller_;
};

TEST_F(LensQueryFlowRouterContextualTaskEnabledTest,
       StartQueryFlow_RoutesToContextualTasks) {
  // Arrange: Set up and create the router.
  EXPECT_CALL(*mock_lens_search_controller_,
              lens_search_contextualization_controller())
      .WillOnce(Return(contextualization_controller_.get()));
  TestLensQueryFlowRouter router(mock_lens_search_controller_.get(),
                                 mock_context_controller_.get());

  GURL example_url("https://example.com");
  std::string page_title = "Title";
  lens::MimeType primary_content_type = lens::MimeType::kAnnotatedPageContent;
  float ui_scale_factor = 1.0f;
  base::TimeTicks invocation_time = base::TimeTicks::Now();

  // Arrange: Create expected contextual input data.
  lens::ContextualInputData expected_input_data;
  expected_input_data.page_url = example_url;
  expected_input_data.page_title = page_title;
  expected_input_data.primary_content_type = primary_content_type;
  expected_input_data.viewport_screenshot = router.GetViewportScreenshot();
  expected_input_data.pdf_current_page = std::nullopt;
  expected_input_data.is_page_context_eligible = true;

  // TODO(crbug.com/463400248): Use contextual tasks image upload config params
  // for Lens requests.
  // Arrange: Create expected image encoding options..
  auto image_upload_config =
      ntp_composebox::FeatureConfig::Get().config.composebox().image_upload();
  lens::ImageEncodingOptions expected_image_options{
      .enable_webp_encoding = image_upload_config.enable_webp_encoding(),
      .max_size = image_upload_config.downscale_max_image_size(),
      .max_height = image_upload_config.downscale_max_image_height(),
      .max_width = image_upload_config.downscale_max_image_width(),
      .compression_quality = image_upload_config.image_compression_quality()};

  // Assert: Create expectation.
  EXPECT_CALL(*router.mock_session_handle(), NotifySessionStarted());
  EXPECT_CALL(*router.mock_session_handle(),
              StartTabContextUploadFlow(
                  _, ContextualInputDataMatches(expected_input_data),
                  ImageEncodingOptionsMatches(expected_image_options)));

  // Act: Start query flow.
  router.StartQueryFlow(router.GetViewportScreenshot(), example_url, page_title,
                        {}, {}, primary_content_type, std::nullopt,
                        ui_scale_factor, invocation_time);
}

TEST_F(LensQueryFlowRouterContextualTaskEnabledTest,
       SendRegionSearch_RoutesToContextualTasks) {
  // Arrange: Set up and create the router.
  TestLensQueryFlowRouter router(mock_lens_search_controller_.get(),
                                 mock_context_controller_.get());

  // Arrange: Set up the parameters.
  base::Time query_start_time = base::Time::Now();
  auto region = lens::mojom::CenterRotatedBox::New();
  lens::LensOverlaySelectionType selection_type =
      lens::LensOverlaySelectionType::REGION_SEARCH;
  std::map<std::string, std::string> additional_params;
  SkBitmap region_bytes;
  region_bytes.allocN32Pixels(10, 10);

  // Arrange: Create expected request info.
  auto expected_request_info = std::make_unique<CreateSearchUrlRequestInfo>();
  expected_request_info->search_url_type = contextual_search::
      ContextualSearchContextController::SearchUrlType::kStandard;
  expected_request_info->query_start_time = query_start_time;
  expected_request_info->lens_overlay_selection_type = selection_type;
  expected_request_info->additional_params = additional_params;
  expected_request_info->image_crop = lens::ImageCrop();

  // Assert: Create expectation to call CreateSearchUrl. We also expect a call
  // to open the side panel, but that is harder to mock, so we omit it for now.
  EXPECT_CALL(*router.mock_session_handle(), NotifySessionStarted());
  // StartTabContextUploadFlow is called as part of OnFinishedAddingTabContext.
  EXPECT_CALL(*router.mock_session_handle(),
              StartTabContextUploadFlow(_, _, _));
  EXPECT_CALL(
      *router.mock_session_handle(),
      CreateSearchUrl(
          CreateSearchUrlRequestInfoMatches(expected_request_info.get()), _))
      .WillOnce(base::test::RunOnceCallback<1>(
          GURL("https://www.google.com/search?q=test")));
  auto* service = static_cast<MockContextualTasksUiService*>(
      contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
          profile_.get()));
  // Clear the mock session handle when the side panel is opened to avoid a
  // dangling pointer.
  EXPECT_CALL(*service,
              StartTaskUiInSidePanel(
                  mock_browser_window_interface_.get(), &mock_tab_interface_,
                  GURL("https://www.google.com/search?q=test"),
                  testing::Pointer(router.mock_session_handle())))
      .WillOnce(testing::InvokeWithoutArgs(
          [&router]() { router.ClearMockSessionHandle(); }));
  EXPECT_CALL(*mock_tab_contextualization_controller_, GetPageContext(_))
      .WillOnce([](lens::TabContextualizationController::GetPageContextCallback
                       callback) { std::move(callback).Run(nullptr); });

  // Act: Call the method.
  router.SendRegionSearch(query_start_time, std::move(region), selection_type,
                          additional_params, region_bytes);
}

TEST_F(LensQueryFlowRouterContextualTaskEnabledTest,
       SendTextOnlyQuery_RoutesToContextualTasks) {
  // Arrange: Set up and create the router.
  EXPECT_CALL(*mock_lens_search_controller_,
              lens_search_contextualization_controller())
      .WillRepeatedly(Return(contextualization_controller_.get()));
  TestLensQueryFlowRouter router(mock_lens_search_controller_.get(),
                                 mock_context_controller_.get());

  // Arrange: Set up the parameters.
  base::Time query_start_time = base::Time::Now();
  std::string query_text = "test query";
  lens::LensOverlaySelectionType selection_type =
      lens::LensOverlaySelectionType::TRANSLATE_CHIP;
  std::map<std::string, std::string> additional_params;

  // Arrange: Create expected request info.
  auto expected_request_info = std::make_unique<CreateSearchUrlRequestInfo>();
  expected_request_info->search_url_type = contextual_search::
      ContextualSearchContextController::SearchUrlType::kStandard;
  expected_request_info->query_text = query_text;
  expected_request_info->query_start_time = query_start_time;
  expected_request_info->lens_overlay_selection_type = selection_type;
  expected_request_info->additional_params = additional_params;
  expected_request_info->image_crop = std::nullopt;

  // Assert: Create expectation to call CreateSearchUrl.
  EXPECT_CALL(*router.mock_session_handle(), NotifySessionStarted());
  // StartTabContextUploadFlow is called as part of OnFinishedAddingTabContext.
  EXPECT_CALL(*router.mock_session_handle(),
              StartTabContextUploadFlow(_, _, _));
  EXPECT_CALL(
      *router.mock_session_handle(),
      CreateSearchUrl(
          CreateSearchUrlRequestInfoMatches(expected_request_info.get()), _))
      .WillOnce(base::test::RunOnceCallback<1>(
          GURL("https://www.google.com/search?q=test")));
  auto* service = static_cast<MockContextualTasksUiService*>(
      contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
          profile_.get()));
  // Clear the mock session handle when the side panel is opened to avoid a
  // dangling pointer.
  EXPECT_CALL(*service,
              StartTaskUiInSidePanel(
                  mock_browser_window_interface_.get(), &mock_tab_interface_,
                  GURL("https://www.google.com/search?q=test"),
                  testing::Pointer(router.mock_session_handle())))
      .WillOnce(testing::InvokeWithoutArgs(
          [&router]() { router.ClearMockSessionHandle(); }));
  EXPECT_CALL(*mock_tab_contextualization_controller_, GetPageContext(_))
      .WillOnce([](lens::TabContextualizationController::GetPageContextCallback
                       callback) { std::move(callback).Run(nullptr); });

  // Act: Call the method.
  router.SendTextOnlyQuery(query_start_time, query_text, selection_type,
                           additional_params);
}

TEST_F(LensQueryFlowRouterContextualTaskEnabledTest,
       SendContextualTextQuery_RoutesToContextualTasks) {
  // Arrange: Set up and create the router.
  TestLensQueryFlowRouter router(mock_lens_search_controller_.get(),
                                 mock_context_controller_.get());

  // Arrange: Set up the parameters.
  base::Time query_start_time = base::Time::Now();
  std::string query_text = "test query";
  lens::LensOverlaySelectionType selection_type =
      lens::LensOverlaySelectionType::MULTIMODAL_SUGGEST_TYPEAHEAD;
  std::map<std::string, std::string> additional_params;

  // Arrange: Create expected request info.
  auto expected_request_info = std::make_unique<CreateSearchUrlRequestInfo>();
  expected_request_info->search_url_type =
      contextual_search::ContextualSearchContextController::SearchUrlType::kAim;
  expected_request_info->query_text = query_text;
  expected_request_info->query_start_time = query_start_time;
  expected_request_info->lens_overlay_selection_type = selection_type;
  expected_request_info->additional_params = additional_params;
  expected_request_info->image_crop = std::nullopt;

  // Assert: Create expectation to call CreateSearchUrl.
  EXPECT_CALL(*router.mock_session_handle(), NotifySessionStarted());
  // StartTabContextUploadFlow is called as part of OnFinishedAddingTabContext.
  EXPECT_CALL(*router.mock_session_handle(),
              StartTabContextUploadFlow(_, _, _));
  EXPECT_CALL(
      *router.mock_session_handle(),
      CreateSearchUrl(
          CreateSearchUrlRequestInfoMatches(expected_request_info.get()), _))
      .WillOnce(base::test::RunOnceCallback<1>(
          GURL("https://www.google.com/search?q=test")));
  auto* service = static_cast<MockContextualTasksUiService*>(
      contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
          profile_.get()));
  // Clear the mock session handle when the side panel is opened to avoid a
  // dangling pointer.
  EXPECT_CALL(*service,
              StartTaskUiInSidePanel(
                  mock_browser_window_interface_.get(), &mock_tab_interface_,
                  GURL("https://www.google.com/search?q=test"),
                  testing::Pointer(router.mock_session_handle())))
      .WillOnce(testing::InvokeWithoutArgs(
          [&router]() { router.ClearMockSessionHandle(); }));
  EXPECT_CALL(*mock_tab_contextualization_controller_, GetPageContext(_))
      .WillOnce([](lens::TabContextualizationController::GetPageContextCallback
                       callback) { std::move(callback).Run(nullptr); });

  // Act: Call the method.
  router.SendContextualTextQuery(query_start_time, query_text, selection_type,
                                 additional_params);
}

TEST_F(LensQueryFlowRouterContextualTaskEnabledTest,
       SendMultimodalRequest_RoutesToContextualTasks) {
  // Arrange: Set up and create the router.
  TestLensQueryFlowRouter router(mock_lens_search_controller_.get(),
                                 mock_context_controller_.get());

  // Arrange: Set up the parameters.
  base::Time query_start_time = base::Time::Now();
  auto region = lens::mojom::CenterRotatedBox::New();
  std::string query_text = "test query";
  lens::LensOverlaySelectionType selection_type =
      lens::LensOverlaySelectionType::MULTIMODAL_SEARCH;
  std::map<std::string, std::string> additional_params;
  SkBitmap region_bytes;
  region_bytes.allocN32Pixels(10, 10);

  // Arrange: Create expected request info.
  auto expected_request_info = std::make_unique<CreateSearchUrlRequestInfo>();
  expected_request_info->search_url_type = contextual_search::
      ContextualSearchContextController::SearchUrlType::kStandard;
  expected_request_info->query_text = query_text;
  expected_request_info->query_start_time = query_start_time;
  expected_request_info->lens_overlay_selection_type = selection_type;
  expected_request_info->additional_params = additional_params;
  expected_request_info->image_crop = lens::ImageCrop();

  // Assert: Create expectation to call CreateSearchUrl. We also expect a call
  // to open the side panel, but that is harder to mock, so we omit it for now.
  EXPECT_CALL(*router.mock_session_handle(), NotifySessionStarted());
  // StartTabContextUploadFlow is called as part of OnFinishedAddingTabContext.
  EXPECT_CALL(*router.mock_session_handle(),
              StartTabContextUploadFlow(_, _, _));
  EXPECT_CALL(
      *router.mock_session_handle(),
      CreateSearchUrl(
          CreateSearchUrlRequestInfoMatches(expected_request_info.get()), _))
      .WillOnce(base::test::RunOnceCallback<1>(
          GURL("https://www.google.com/search?q=test")));
  auto* service = static_cast<MockContextualTasksUiService*>(
      contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
          profile_.get()));
  // Clear the mock session handle when the side panel is opened to avoid a
  // dangling pointer.
  EXPECT_CALL(*service,
              StartTaskUiInSidePanel(
                  mock_browser_window_interface_.get(), &mock_tab_interface_,
                  GURL("https://www.google.com/search?q=test"),
                  testing::Pointer(router.mock_session_handle())))
      .WillOnce(testing::InvokeWithoutArgs(
          [&router]() { router.ClearMockSessionHandle(); }));
  EXPECT_CALL(*mock_tab_contextualization_controller_, GetPageContext(_))
      .WillOnce([](lens::TabContextualizationController::GetPageContextCallback
                       callback) { std::move(callback).Run(nullptr); });

  // Act: Call the method.
  router.SendMultimodalRequest(query_start_time, std::move(region), query_text,
                               selection_type, additional_params, region_bytes);
}

TEST_F(LensQueryFlowRouterContextualTaskEnabledTest,
       GetSuggestInputs_RoutesToContextualTasks) {
  // Arrange
  EXPECT_CALL(*mock_lens_search_controller_,
              lens_search_contextualization_controller())
      .WillOnce(Return(contextualization_controller_.get()));
  TestLensQueryFlowRouter router(mock_lens_search_controller_.get(),
                                 mock_context_controller_.get());

  // Start the query flow to initialize the session handle.
  GURL example_url("https://example.com");
  std::string page_title = "Title";
  lens::MimeType primary_content_type = lens::MimeType::kAnnotatedPageContent;
  float ui_scale_factor = 1.0f;
  base::TimeTicks invocation_time = base::TimeTicks::Now();

  EXPECT_CALL(*router.mock_session_handle(), NotifySessionStarted());
  EXPECT_CALL(*mock_context_controller_, AddObserver(&router));
  EXPECT_CALL(*router.mock_session_handle(),
              StartTabContextUploadFlow(_, _, _));

  router.StartQueryFlow(router.GetViewportScreenshot(), example_url, page_title,
                        {}, {}, primary_content_type, std::nullopt,
                        ui_scale_factor, invocation_time);

  // Test GetSuggestInputs.
  lens::proto::LensOverlaySuggestInputs expected_inputs;
  expected_inputs.set_encoded_request_id("test_id");
  EXPECT_CALL(*router.mock_session_handle(), GetSuggestInputs())
      .WillOnce(Return(expected_inputs));

  // Act
  auto result = router.GetSuggestInputs();

  // Assert
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->encoded_request_id(), "test_id");
}

TEST_F(LensQueryFlowRouterContextualTaskEnabledTest,
       SetSuggestInputsReadyCallback_RoutesToContextualTasks) {
  // Arrange
  EXPECT_CALL(*mock_lens_search_controller_,
              lens_search_contextualization_controller())
      .WillOnce(Return(contextualization_controller_.get()));
  TestLensQueryFlowRouter router(mock_lens_search_controller_.get(),
                                 mock_context_controller_.get());

  // Start the query flow to initialize the session handle.
  GURL example_url("https://example.com");
  std::string page_title = "Title";
  lens::MimeType primary_content_type = lens::MimeType::kAnnotatedPageContent;
  float ui_scale_factor = 1.0f;
  base::TimeTicks invocation_time = base::TimeTicks::Now();

  EXPECT_CALL(*router.mock_session_handle(), NotifySessionStarted());
  EXPECT_CALL(*mock_context_controller_, AddObserver(&router));
  EXPECT_CALL(*router.mock_session_handle(),
              StartTabContextUploadFlow(_, _, _));

  router.StartQueryFlow(router.GetViewportScreenshot(), example_url, page_title,
                        {}, {}, primary_content_type, std::nullopt,
                        ui_scale_factor, invocation_time);

  // Test SetSuggestInputsReadyCallback.
  // The router checks if inputs are ready first. Return empty to simulate not
  // ready.
  EXPECT_CALL(*router.mock_session_handle(), GetSuggestInputs())
      .WillOnce(Return(std::nullopt));
  // The observer is added again to listen for updates.
  EXPECT_CALL(*mock_context_controller_, AddObserver(&router));

  // Act
  router.SetSuggestInputsReadyCallback(base::DoNothing());
}

}  // namespace lens
