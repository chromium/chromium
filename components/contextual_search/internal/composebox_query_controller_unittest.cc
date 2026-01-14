// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/internal/composebox_query_controller.h"

#include <memory>
#include <optional>
#include <string>

#include "base/base64url.h"
#include "base/test/bind.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/version_info/channel.h"
#include "components/contextual_search/internal/test_composebox_query_controller.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_bitmap_processing.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_url_utils.h"
#include "components/omnibox/composebox/composebox_query.mojom.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/variations/variations_client.h"
#include "net/base/url_util.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "third_party/lens_server_proto/lens_overlay_contextual_inputs.pb.h"
#include "third_party/lens_server_proto/lens_overlay_platform.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "third_party/lens_server_proto/lens_overlay_surface.pb.h"
#include "third_party/lens_server_proto/lens_overlay_visual_search_interaction_data.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia_operations.h"

#if !BUILDFLAG(IS_IOS)
#include "ui/gfx/codec/jpeg_codec.h"
#endif  // !BUILDFLAG(IS_IOS)

constexpr char kContextualInputsParameterKey[] = "cinpts";
constexpr char kQuerySubmissionTimeQueryParameter[] = "qsubts";
constexpr char kClientUploadDurationQueryParameter[] = "cud";
constexpr char kSessionIdQueryParameterKey[] = "gsessionid";
constexpr char kSearchModeQueryParameterKey[] = "udm";
constexpr char kVariationsHeaderKey[] = "X-Client-Data";
constexpr char kTestUser[] = "test_user@gmail.com";
constexpr char kTestSearchSessionId[] = "test_search_session_id";
constexpr char kTestServerSessionId[] = "test_server_session_id";
constexpr char kLocale[] = "en-US";
constexpr char kRegion[] = "US";
constexpr char kTimeZone[] = "America/Los_Angeles";
constexpr char kRequestIdParameterKey[] = "vsrid";
constexpr char kVisualSearchInteractionDataParameterKey[] = "vsint";
constexpr char kVisualInputTypeParameterKey[] = "vit";
constexpr char kLnsSurfaceParameterKey[] = "lns_surface";
constexpr char kTestCellAddress[] = "test_cell_address";
constexpr char kTestServerAddress[] = "test_server_address";
constexpr char kAimUdmQueryParameterValue[] = "50";
constexpr char kMultimodalUdmQueryParameterValue[] = "24";
constexpr char kUnimodalUdmQueryParameterValue[] = "26";

#if BUILDFLAG(IS_ANDROID)
constexpr lens::CompressionType kExpectedPdfCompressionType =
    lens::CompressionType::UNCOMPRESSED;
#else
constexpr lens::CompressionType kExpectedPdfCompressionType =
    lens::CompressionType::ZSTD;
#endif  // !BUILDFLAG(IS_ANDROID)

base::Time kTestQueryStartTime =
    base::Time::FromMillisecondsSinceUnixEpoch(1000);

namespace contextual_search {

using FileUploadStatusTuple = std::tuple<base::UnguessableToken,
                                         lens::MimeType,
                                         FileUploadStatus,
                                         std::optional<FileUploadErrorType>>;
using CreateSearchUrlRequestInfo =
    ComposeboxQueryController::CreateSearchUrlRequestInfo;
using CreateClientToAimRequestInfo =
    ComposeboxQueryController::CreateClientToAimRequestInfo;

using base::test::EqualsProto;

class ComposeboxQueryControllerTest
    : public testing::Test,
      public ComposeboxQueryController::FileUploadStatusObserver {
 public:
  using QueryControllerState =
      TestComposeboxQueryController::QueryControllerState;

  ComposeboxQueryControllerTest() = default;
  ~ComposeboxQueryControllerTest() override = default;

  void CreateController(
      bool send_lns_surface,
      bool suppress_lns_surface_param_if_no_image = true,
      bool enable_multi_context_input_flow = false,
      bool enable_viewport_images = true,
      bool use_separate_request_ids_for_multi_context_viewport_images = true,
      bool enable_cluster_info_ttl = false,
      bool prioritize_suggestions_for_the_first_attached_document = false,
      bool attach_page_title_and_url_to_suggest_requests = false) {
    // Create the config params.
    auto config_params =
        std::make_unique<ContextualSearchContextController::ConfigParams>();
    config_params->send_lns_surface = send_lns_surface;
    config_params->suppress_lns_surface_param_if_no_image =
        suppress_lns_surface_param_if_no_image;
    config_params->enable_multi_context_input_flow =
        enable_multi_context_input_flow;
    config_params->enable_viewport_images = enable_viewport_images;
    config_params->use_separate_request_ids_for_multi_context_viewport_images =
        use_separate_request_ids_for_multi_context_viewport_images;
    config_params->use_separate_request_ids_for_multi_context_viewport_images =
        use_separate_request_ids_for_multi_context_viewport_images;
    config_params->prioritize_suggestions_for_the_first_attached_document =
        prioritize_suggestions_for_the_first_attached_document;
    config_params->attach_page_title_and_url_to_suggest_requests =
        attach_page_title_and_url_to_suggest_requests;

    // Create the controller.
    controller_ = std::make_unique<TestComposeboxQueryController>(
        identity_manager(), shared_url_loader_factory_,
        version_info::Channel::UNKNOWN, kLocale, template_url_service(),
        fake_variations_client_.get(), std::move(config_params),
        enable_cluster_info_ttl);
    controller_->AddObserver(this);

    // Initialize the cluster info response.
    lens::LensOverlayServerClusterInfoResponse cluster_info_response;
    cluster_info_response.set_search_session_id(kTestSearchSessionId);
    cluster_info_response.set_server_session_id(kTestServerSessionId);
    cluster_info_response.mutable_routing_info()->set_cell_address(
        kTestCellAddress);
    cluster_info_response.mutable_routing_info()->set_server_address(
        kTestServerAddress);
    controller_->set_fake_cluster_info_response(cluster_info_response);

    controller().set_on_query_controller_state_changed_callback(
        base::BindRepeating(
            &ComposeboxQueryControllerTest::OnQueryControllerStateChanged,
            base::Unretained(this)));
  }

  void SetUp() override {
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_factory_);

    in_process_data_decoder_ =
        std::make_unique<data_decoder::test::InProcessDataDecoder>();

    icu::TimeZone::adoptDefault(
        icu::TimeZone::createTimeZone(icu::UnicodeString(kTimeZone)));
    UErrorCode error_code = U_ZERO_ERROR;
    icu::Locale::setDefault(icu::Locale(kLocale), error_code);
    ASSERT_TRUE(U_SUCCESS(error_code));

    fake_variations_client_ = std::make_unique<FakeVariationsClient>();
    CreateController(/*send_lns_surface=*/false);
  }

  void TearDown() override {
    controller_->RemoveObserver(this);
    while (!controller_state_future_.IsEmpty()) {
      controller_state_future_.Take();
    }
    while (!file_upload_status_future_.IsEmpty()) {
      file_upload_status_future_.Take();
    }
  }

  void WaitForClusterInfo(QueryControllerState expected_state =
                              QueryControllerState::kClusterInfoReceived,
                          int fetch_request_count = 1) {
    EXPECT_EQ(QueryControllerState::kAwaitingClusterInfoResponse,
              controller_state_future_.Take());
    EXPECT_EQ(QueryControllerState::kAwaitingClusterInfoResponse,
              controller().query_controller_state());

    EXPECT_EQ(expected_state, controller_state_future_.Take());
    EXPECT_EQ(expected_state, controller().query_controller_state());

    EXPECT_EQ(controller().num_cluster_info_fetch_requests_sent(),
              fetch_request_count);
    // The cluster info request should have the cors variations header.
    EXPECT_THAT(controller().last_sent_cors_exempt_headers(),
                testing::Contains(kVariationsHeaderKey));
  }

  void StartPdfFileUploadFlow(
      const base::UnguessableToken& file_token,
      const std::vector<uint8_t>& file_data,
      std::optional<int64_t> context_id = std::nullopt) {
    std::unique_ptr<lens::ContextualInputData> input_data =
        std::make_unique<lens::ContextualInputData>();
    input_data->primary_content_type = lens::MimeType::kPdf;
    input_data->context_input = std::vector<lens::ContextualInput>();
    input_data->context_input->push_back(
        lens::ContextualInput(file_data, lens::MimeType::kPdf));
    input_data->context_id = context_id;

    controller().StartFileUploadFlow(file_token, std::move(input_data),
                                     /*image_options=*/std::nullopt);
  }

  void StartImageFileUploadFlow(
      const base::UnguessableToken& file_token,
      const std::vector<uint8_t>& file_data,
      std::optional<lens::ImageEncodingOptions> image_options = std::nullopt,
      std::optional<int64_t> context_id = std::nullopt) {
    std::unique_ptr<lens::ContextualInputData> input_data =
        std::make_unique<lens::ContextualInputData>();
    input_data->primary_content_type = lens::MimeType::kImage;
    input_data->context_input = std::vector<lens::ContextualInput>();
    input_data->context_input->push_back(
        lens::ContextualInput(file_data, lens::MimeType::kImage));
    input_data->context_id = context_id;

    controller().StartFileUploadFlow(file_token, std::move(input_data),
                                     image_options);
  }

  void WaitForFileUpload(
      const base::UnguessableToken& file_token,
      lens::MimeType mime_type,
      FileUploadStatus expected_status = FileUploadStatus::kUploadSuccessful,
      std::optional<FileUploadErrorType> expected_error_type = std::nullopt,
      bool expect_suggest_signals_ready = true) {
    FileUploadStatusTuple processing_file_upload_status =
        file_upload_status_future_.Take();
    EXPECT_EQ(file_token, std::get<0>(processing_file_upload_status));
    EXPECT_EQ(mime_type, std::get<1>(processing_file_upload_status));
    EXPECT_EQ(FileUploadStatus::kProcessing,
              std::get<2>(processing_file_upload_status));
    EXPECT_EQ(std::nullopt, std::get<3>(processing_file_upload_status));

    if (expect_suggest_signals_ready) {
      FileUploadStatusTuple processing_suggest_file_upload_status =
          file_upload_status_future_.Take();
      EXPECT_EQ(file_token, std::get<0>(processing_suggest_file_upload_status));
      EXPECT_EQ(mime_type, std::get<1>(processing_suggest_file_upload_status));
      EXPECT_EQ(FileUploadStatus::kProcessingSuggestSignalsReady,
                std::get<2>(processing_suggest_file_upload_status));
      EXPECT_EQ(std::nullopt,
                std::get<3>(processing_suggest_file_upload_status));
    }

    if (expected_status != FileUploadStatus::kValidationFailed) {
      // For client-side validation failures, the state will never change to
      // kUploadStarted.
      FileUploadStatusTuple upload_started_file_upload_status =
          file_upload_status_future_.Take();
      EXPECT_EQ(file_token, std::get<0>(upload_started_file_upload_status));
      EXPECT_EQ(mime_type, std::get<1>(upload_started_file_upload_status));
      EXPECT_EQ(FileUploadStatus::kUploadStarted,
                std::get<2>(upload_started_file_upload_status));
      EXPECT_EQ(std::nullopt, std::get<3>(upload_started_file_upload_status));
    }

    FileUploadStatusTuple final_file_upload_status =
        file_upload_status_future_.Take();
    EXPECT_EQ(file_token, std::get<0>(final_file_upload_status));
    EXPECT_EQ(mime_type, std::get<1>(final_file_upload_status));
    EXPECT_EQ(expected_status, std::get<2>(final_file_upload_status));
    EXPECT_EQ(expected_error_type, std::get<3>(final_file_upload_status));

    if (expected_status == FileUploadStatus::kValidationFailed) {
      // For client-side validation failures, the file upload request will not
      // be sent.
      EXPECT_EQ(controller().num_file_upload_requests_sent(), 0);
    } else {
      EXPECT_GT(controller().num_file_upload_requests_sent(), 0);
      EXPECT_THAT(GetGsessionIdFromUrl(controller().last_sent_fetch_url()),
                  testing::Optional(std::string(kTestServerSessionId)));
      // The file upload request should have the cors variations header.
      EXPECT_THAT(controller().last_sent_cors_exempt_headers(),
                  testing::Contains(kVariationsHeaderKey));
    }
  }

  // Initialize controller, ensuring cluster info is set up.
  void StartSession() {
    controller().InitializeIfNeeded();
    WaitForClusterInfo();
  }

  base::UnguessableToken UploadSimpleTestAttachment(lens::MimeType mime_type) {
    // Act: Start the file upload flow.
    auto file_token = base::UnguessableToken::Create();
    switch (mime_type) {
      case lens::MimeType::kPdf: {
        StartPdfFileUploadFlow(file_token,
                               /*file_data=*/std::vector<uint8_t>());
        WaitForFileUpload(file_token, mime_type);
        break;
      }

      case lens::MimeType::kImage: {
        lens::ImageEncodingOptions image_options{.max_size = 1000,
                                                 .max_height = 10,
                                                 .max_width = 10,
                                                 .compression_quality = 10};
        StartImageFileUploadFlow(file_token, GetSimpleJPGBytes(),
                                 image_options);
        // NOTE: WaitForFileUpload() never completes/hangs the test.
        break;
      }

      case lens::MimeType::kAnnotatedPageContent: {
        auto input_data = std::make_unique<lens::ContextualInputData>();
        input_data->primary_content_type =
            lens::MimeType::kAnnotatedPageContent;
        input_data->context_input = std::vector<lens::ContextualInput>();
        input_data->page_url = GURL("https://page.url");
        input_data->page_title = "Page Title";
        input_data->context_input->emplace_back(lens::ContextualInput(
            std::vector<uint8_t>(), lens::MimeType::kAnnotatedPageContent));
        input_data->is_page_context_eligible = true;
        controller().StartFileUploadFlow(file_token, std::move(input_data),
                                         std::nullopt);
        WaitForFileUpload(file_token, mime_type);
        break;
      }

      default:
        EXPECT_TRUE(false) << "Unsupported Lens MIME Type";
    }

    // Assert: Validate file upload request and status changes.
    EXPECT_TRUE(controller().GetFileInfoForTesting(file_token));
    return file_token;
  }

  std::string GetEncodedRequestInfoForToken(
      const base::UnguessableToken& token) {
    return lens::Base64EncodeRequestId(
        controller().GetFileInfoForTesting(token)->GetRequestIdForTesting());
  }

  TestComposeboxQueryController& controller() { return *controller_; }

  void OnQueryControllerStateChanged(QueryControllerState new_state) {
    controller_state_future_.AddValue(new_state);
  }

  // ComposeboxQueryController::FileUploadStatusObserver:
  void OnFileUploadStatusChanged(
      const base::UnguessableToken& file_token,
      lens::MimeType mime_type,
      FileUploadStatus file_upload_status,
      const std::optional<FileUploadErrorType>& error_type) override {
    file_upload_status_future_.AddValue(file_token, mime_type,
                                        file_upload_status, error_type);
  }

#if !BUILDFLAG(IS_IOS)
  std::vector<uint8_t> CreateJPGBytes(int width, int height) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    bitmap.eraseColor(SK_ColorRED);  // Fill with a solid color
    auto image_bytes = gfx::JPEGCodec::Encode(bitmap, 100);
    return image_bytes.value();
  }
#endif  // !BUILDFLAG(IS_IOS)

  std::vector<uint8_t> GetSimpleJPGBytes() {
    // Returns 1x1 progressive jpg image.
    // https://stackoverflow.com/questions/2253404
    return {
        0xFF, 0xD8, 0xFF, 0xDB, 0x00, 0x43, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xFF,
        0xC2, 0x00, 0x0B, 0x08, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x11, 0x00,
        0xFF, 0xC4, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xFF, 0xDA,
        0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01, 0x3F, 0xFF, 0xD9,
    };
  }

  lens::LensOverlayRequestId DecodeRequestIdFromVsrid(std::string vsrid_param) {
    std::string serialized_proto;
    EXPECT_TRUE(base::Base64UrlDecode(
        vsrid_param, base::Base64UrlDecodePolicy::DISALLOW_PADDING,
        &serialized_proto));
    lens::LensOverlayRequestId proto;
    EXPECT_TRUE(proto.ParseFromString(serialized_proto));
    return proto;
  }

  lens::LensOverlayRequestId GetRequestIdFromUrl(std::string url_string) {
    GURL url = GURL(url_string);
    std::string vsrid_param;
    EXPECT_TRUE(
        net::GetValueForKeyInQuery(url, kRequestIdParameterKey, &vsrid_param));
    return DecodeRequestIdFromVsrid(vsrid_param);
  }

  lens::LensOverlayContextualInputs GetContextualInputsFromUrl(
      std::string url_string) {
    GURL url = GURL(url_string);
    std::string contextual_inputs_param;
    EXPECT_TRUE(net::GetValueForKeyInQuery(url, kContextualInputsParameterKey,
                                           &contextual_inputs_param));
    std::string serialized_proto;
    EXPECT_TRUE(base::Base64UrlDecode(
        contextual_inputs_param, base::Base64UrlDecodePolicy::DISALLOW_PADDING,
        &serialized_proto));
    lens::LensOverlayContextualInputs proto;
    EXPECT_TRUE(proto.ParseFromString(serialized_proto));
    return proto;
  }

  lens::LensOverlayVisualSearchInteractionData GetVsintFromUrl(
      const GURL& url) {
    std::string vsint_param;
    EXPECT_TRUE(net::GetValueForKeyInQuery(
        url, kVisualSearchInteractionDataParameterKey, &vsint_param));
    std::string serialized_proto;
    EXPECT_TRUE(base::Base64UrlDecode(
        vsint_param, base::Base64UrlDecodePolicy::DISALLOW_PADDING,
        &serialized_proto));
    lens::LensOverlayVisualSearchInteractionData proto;
    EXPECT_TRUE(proto.ParseFromString(serialized_proto));
    return proto;
  }

 protected:
  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  TemplateURLService* template_url_service() {
    return search_engines_test_environment_.template_url_service();
  }

  // Returns an AccessTokenInfo with valid information that can be used for
  // completing access token requests.
  const signin::AccessTokenInfo& access_token_info() const {
    return access_token_info_;
  }

  // Returns the gsessionid parameter from the given url.
  std::optional<std::string> GetGsessionIdFromUrl(GURL url) {
    std::string gsessionid_param;
    bool has_gsessionid_param = net::GetValueForKeyInQuery(
        url, kSessionIdQueryParameterKey, &gsessionid_param);
    if (has_gsessionid_param) {
      return gsessionid_param;
    }
    return std::nullopt;
  }

  // Returns the task environment.
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  base::test::RepeatingTestFuture<QueryControllerState>
      controller_state_future_;
  base::test::RepeatingTestFuture<base::UnguessableToken,
                                  lens::MimeType,
                                  FileUploadStatus,
                                  std::optional<FileUploadErrorType>>
      file_upload_status_future_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  network::TestURLLoaderFactory test_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<FakeVariationsClient> fake_variations_client_;
  std::unique_ptr<TestComposeboxQueryController> controller_;
  std::unique_ptr<data_decoder::test::InProcessDataDecoder>
      in_process_data_decoder_;
  signin::AccessTokenInfo access_token_info_{"access_token", base::Time::Max(),
                                             "id_token"};
};

TEST_F(ComposeboxQueryControllerTest,
       InitializeIfNeededIssuesClusterInfoRequest) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();
}

TEST_F(ComposeboxQueryControllerTest, InitializeIfNeededSecondTimeDoesNothing) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the session again.
  controller().InitializeIfNeeded();

  // Assert: No cluster info request is made.
  EXPECT_TRUE(controller_state_future_.IsEmpty());
}

TEST_F(ComposeboxQueryControllerTest,
       InitializeIfNeededIssuesClusterInfoRequestWithOAuth) {
  // Arrange: Make primary account available.
  identity_test_env()->MakePrimaryAccountAvailable(
      kTestUser, signin::ConsentLevel::kSignin);

  // Act: Start the session.
  controller().InitializeIfNeeded();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);

  // Assert: Validate cluster info request and state changes
  WaitForClusterInfo();
}

TEST_F(ComposeboxQueryControllerTest,
       InitializeIfNeededIssuesClusterInfoRequestFailure) {
  // Arrange: Simulate an error in the cluster info request.
  controller().set_next_cluster_info_request_should_return_error(true);

  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo(
      /*expected_state=*/QueryControllerState::kClusterInfoInvalid);
}

TEST_F(ComposeboxQueryControllerTest, UploadFileRequestFailure) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Arrange: Simulate a failure in the file upload request.
  controller().set_next_file_upload_request_should_return_error(true);

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf,
                    /*expected_status=*/FileUploadStatus::kUploadFailed,
                    /*expected_error_type=*/FileUploadErrorType::kServerError);

  // Assert: The suggest inputs are empty.
  auto suggest_inputs = controller().CreateSuggestInputs({file_token});
  EXPECT_FALSE(suggest_inputs->has_search_session_id());
  EXPECT_FALSE(suggest_inputs->has_encoded_request_id());
}

TEST_F(ComposeboxQueryControllerTest,
       UploadFileWithoutClusterInfoNeverHasSuggestReady) {
  // Arrange: Simulate an error in the cluster info request.
  controller().set_next_cluster_info_request_should_return_error(true);

  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>());

  WaitForClusterInfo(
      /*expected_state=*/QueryControllerState::kClusterInfoInvalid);

  // Assert: Validate file upload request and status changes.
  FileUploadStatusTuple processing_file_upload_status =
      file_upload_status_future_.Take();
  EXPECT_EQ(file_token, std::get<0>(processing_file_upload_status));
  EXPECT_EQ(FileUploadStatus::kProcessing,
            std::get<2>(processing_file_upload_status));
  EXPECT_EQ(std::nullopt, std::get<3>(processing_file_upload_status));

  // Assert: file_upload_status_future_ is empty.
  EXPECT_TRUE(file_upload_status_future_.IsEmpty());
}

#if !BUILDFLAG(IS_IOS)
TEST_F(ComposeboxQueryControllerTest, UploadImageFileRequestSuccess) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  std::vector<uint8_t> image_bytes = CreateJPGBytes(100, 100);
  lens::ImageEncodingOptions image_options{.max_size = 1000000,
                                           .max_height = 1000,
                                           .max_width = 1000,
                                           .compression_quality = 30};
  StartImageFileUploadFlow(file_token, image_bytes, image_options);

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kImage);
  // Validate the file upload request payload.
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .image_data()
                .image_metadata()
                .width(),
            100);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .image_data()
                .image_metadata()
                .height(),
            100);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->client_logs()
                .phase_latencies_metadata()
                .phase_size(),
            1);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->client_logs()
                .phase_latencies_metadata()
                .phase(0)
                .image_encode_data()
                .encoded_image_size_bytes(),
            360);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .request_context()
                .request_id()
                .media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  // Check that the vsrid matches that for an image upload.
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .image_sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .long_context_id(),
            0);
  // Check that the routing info is in the vsrid.
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .routing_info()
                .cell_address(),
            kTestCellAddress);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .routing_info()
                .server_address(),
            kTestServerAddress);

  auto suggest_inputs = controller().CreateSuggestInputs({file_token});
  EXPECT_EQ(suggest_inputs->search_session_id(), kTestSearchSessionId);
  EXPECT_TRUE(suggest_inputs->send_gsession_vsrid_for_contextual_suggest());
}

TEST_F(ComposeboxQueryControllerTest,
       UploadFileRemainsActiveAfterClusterInfoExpiration) {
  CreateController(
      /*send_lns_surface=*/false,
      /*suppress_lns_surface_param_if_no_image=*/true,
      /*enable_multi_context_input_flow=*/false,
      /*enable_viewport_images=*/true,
      /*use_separate_request_ids_for_multi_context_viewport_images=*/true,
      /*enable_cluster_info_ttl=*/true);

  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Act: Fast forward time to expire the cluster info.
  // The default cluster info lifetime is 1 hour.
  task_environment().FastForwardBy(base::Hours(1) + base::Seconds(1));

  // Assert: Validate cluster info request and state changes.
  // The cluster info should be re-fetched.
  // First, the state becomes kClusterInfoInvalid.
  EXPECT_EQ(QueryControllerState::kClusterInfoInvalid,
            controller_state_future_.Take());
  // Then, it starts fetching and becomes kAwaitingClusterInfoResponse.
  EXPECT_EQ(QueryControllerState::kAwaitingClusterInfoResponse,
            controller_state_future_.Take());
  // Finally, it receives the response and becomes kClusterInfoReceived.
  EXPECT_EQ(QueryControllerState::kClusterInfoReceived,
            controller_state_future_.Take());

  EXPECT_GE(controller().num_cluster_info_fetch_requests_sent(), 2);

  // Assert: The file should still be in the active files map.
  ASSERT_TRUE(controller().GetFileInfoForTesting(file_token));
  // The file status should be kUploadExpired.
  EXPECT_EQ(controller().GetFileInfoForTesting(file_token)->upload_status,
            FileUploadStatus::kUploadExpired);
}

TEST_F(ComposeboxQueryControllerTest,
       UploadPdfFileRequestWithContextIdMigrationEnabled_SetsContextId) {
  CreateController(
      /*send_lns_surface=*/false,
      /*suppress_lns_surface_param_if_no_image=*/true,
      /*enable_multi_context_input_flow=*/true,
      /*enable_viewport_images=*/true,
      /*use_separate_request_ids_for_multi_context_viewport_images=*/true,
      /*enable_cluster_info_ttl=*/false,
      /*prioritize_suggestions_for_the_first_attached_document=*/false,
      /*enable_context_id_migration=*/true);
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  int64_t context_id = 12345;
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>(), context_id);

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);
  // Validate the file upload request payload.
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .payload()
                .content()
                .content_data(0)
                .content_type(),
            lens::ContentData::CONTENT_TYPE_PDF);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .payload()
                .content()
                .content_data(0)
                .compression_type(),
            kExpectedPdfCompressionType);
  // Check that the vsrid matches that for a pdf upload using the context_id
  // migration flow.
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .image_sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .long_context_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .context_id(),
            context_id);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .request_context()
                .request_id()
                .sequence_id(),
            1);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .request_context()
                .request_id()
                .image_sequence_id(),
            1);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .request_context()
                .request_id()
                .long_context_id(),
            1);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .request_context()
                .request_id()
                .context_id(),
            context_id);
  // Check that the routing info is in the vsrid.
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .routing_info()
                .cell_address(),
            kTestCellAddress);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .routing_info()
                .server_address(),
            kTestServerAddress);

  auto suggest_inputs = controller().CreateSuggestInputs({file_token});
  EXPECT_EQ(suggest_inputs->search_session_id(), kTestSearchSessionId);
  EXPECT_TRUE(suggest_inputs->send_gsession_vsrid_for_contextual_suggest());
}

TEST_F(ComposeboxQueryControllerTest,
       UploadPdfFileRequestWithContextIdMigrationDisabled_IncrementsRequestId) {
  CreateController(
      /*send_lns_surface=*/false,
      /*suppress_lns_surface_param_if_no_image=*/true,
      /*enable_multi_context_input_flow=*/true,
      /*enable_viewport_images=*/true,
      /*use_separate_request_ids_for_multi_context_viewport_images=*/true,
      /*enable_cluster_info_ttl=*/false,
      /*prioritize_suggestions_for_the_first_attached_document=*/false,
      /*enable_context_id_migration=*/false);
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  int64_t context_id = 12345;
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>(), context_id);

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Capture the first request ID.
  auto* first_file_info = controller().GetFileInfoForTesting(file_token);
  ASSERT_TRUE(first_file_info);
  auto first_request_id = first_file_info->GetRequestIdForTesting();
  EXPECT_EQ(first_request_id.sequence_id(), 1);
  EXPECT_EQ(first_request_id.context_id(), context_id);

  // Act: Start the file upload flow again with the same context ID (re-upload).
  const base::UnguessableToken file_token_2 = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token_2,
                         /*file_data=*/std::vector<uint8_t>(), context_id);

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token_2, lens::MimeType::kPdf);

  // Capture the second request ID.
  auto* second_file_info = controller().GetFileInfoForTesting(file_token_2);
  ASSERT_TRUE(second_file_info);
  auto second_request_id = second_file_info->GetRequestIdForTesting();

  // Verify that the request ID was incremented.
  EXPECT_EQ(second_request_id.sequence_id(), 2);
  EXPECT_EQ(second_request_id.context_id(), context_id);
  EXPECT_EQ(second_request_id.uuid(), first_request_id.uuid());
  EXPECT_NE(second_request_id.analytics_id(), first_request_id.analytics_id());
}

TEST_F(ComposeboxQueryControllerTest,
       UploadPdfFileRequestWithContextIdMigrationDisabled_SetsContextId) {
  CreateController(
      /*send_lns_surface=*/false,
      /*suppress_lns_surface_param_if_no_image=*/true,
      /*enable_multi_context_input_flow=*/true,
      /*enable_viewport_images=*/true,
      /*use_separate_request_ids_for_multi_context_viewport_images=*/true,
      /*enable_cluster_info_ttl=*/false,
      /*prioritize_suggestions_for_the_first_attached_document=*/false,
      /*enable_context_id_migration=*/false);
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  int64_t context_id = 12345;
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>(), context_id);

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Check that the vsrid matches that for a pdf upload.
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .image_sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .long_context_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .context_id(),
            context_id);
}

TEST_F(ComposeboxQueryControllerTest, UploadEmptyImageFileRequestFailure) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  std::vector<uint8_t> image_bytes = std::vector<uint8_t>();
  lens::ImageEncodingOptions image_options{.max_size = 1000000,
                                           .max_height = 1000,
                                           .max_width = 1000,
                                           .compression_quality = 30};
  StartImageFileUploadFlow(file_token, image_bytes, image_options);

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kImage,
                    FileUploadStatus::kValidationFailed,
                    FileUploadErrorType::kImageProcessingError);
}
#endif  // !BUILDFLAG(IS_IOS)

TEST_F(ComposeboxQueryControllerTest, UploadPdfFileRequestSuccess) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);
  // Validate the file upload request payload.
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .payload()
                .content()
                .content_data(0)
                .content_type(),
            lens::ContentData::CONTENT_TYPE_PDF);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .payload()
                .content()
                .content_data(0)
                .compression_type(),
            kExpectedPdfCompressionType);
  // Check that the vsrid matches that for a pdf upload.
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .image_sequence_id(),
            0);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .long_context_id(),
            1);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .request_context()
                .request_id()
                .sequence_id(),
            1);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .request_context()
                .request_id()
                .image_sequence_id(),
            0);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .request_context()
                .request_id()
                .long_context_id(),
            1);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .request_context()
                .request_id()
                .media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_PDF);
  // Check that the routing info is in the vsrid.
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .routing_info()
                .cell_address(),
            kTestCellAddress);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .routing_info()
                .server_address(),
            kTestServerAddress);
}

TEST_F(ComposeboxQueryControllerTest, UploadPageContextPdfFileRequestSuccess) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow with multiple context inputs and page
  // context params.
  GURL page_url = GURL("https://www.test.com");
  std::string page_title = "Test Page";
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  std::unique_ptr<lens::ContextualInputData> input_data =
      std::make_unique<lens::ContextualInputData>();
  input_data->primary_content_type = lens::MimeType::kPdf;
  input_data->context_input = std::vector<lens::ContextualInput>();
  input_data->page_url = page_url;
  input_data->page_title = page_title;
  input_data->context_input->push_back(
      lens::ContextualInput(std::vector<uint8_t>(), lens::MimeType::kPdf));
  input_data->context_input->push_back(
      lens::ContextualInput(std::vector<uint8_t>(), lens::MimeType::kPdf));
  controller().StartFileUploadFlow(file_token, std::move(input_data),
                                   /*image_options=*/std::nullopt);

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);
  // Validate the file upload request payload.
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .payload()
                .content()
                .content_data(0)
                .content_type(),
            lens::ContentData::CONTENT_TYPE_PDF);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .payload()
                .content()
                .content_data(1)
                .content_type(),
            lens::ContentData::CONTENT_TYPE_PDF);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .payload()
                .content()
                .content_data(0)
                .compression_type(),
            kExpectedPdfCompressionType);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .payload()
                .content()
                .content_data(1)
                .compression_type(),
            kExpectedPdfCompressionType);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .payload()
                .content()
                .webpage_title(),
            page_title);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .payload()
                .content()
                .webpage_url(),
            page_url.spec());
  // Check that the vsrid matches that for a pdf upload.
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .image_sequence_id(),
            0);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .long_context_id(),
            1);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .request_context()
                .request_id()
                .sequence_id(),
            1);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .request_context()
                .request_id()
                .image_sequence_id(),
            0);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .request_context()
                .request_id()
                .long_context_id(),
            1);
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .request_context()
                .request_id()
                .media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_PDF);
  // Check that the routing info is in the vsrid.
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .routing_info()
                .cell_address(),
            kTestCellAddress);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .routing_info()
                .server_address(),
            kTestServerAddress);
}

#if !BUILDFLAG(IS_IOS)
TEST_F(
    ComposeboxQueryControllerTest,
    UploadPageContextPdfFileWithViewportMultiContextSeparateRequestIdsRequestSuccess) {
  CreateController(
      /*send_lns_surface=*/false,
      /*suppress_lns_surface_param_if_no_image=*/true,
      /*enable_multi_context_input_flow=*/true,
      /*enable_viewport_images=*/true,
      /*use_separate_request_ids_for_multi_context_viewport_images=*/true);

  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow with multiple context inputs and page
  // context params.
  GURL page_url = GURL("https://www.test.com");
  std::string page_title = "Test Page";
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  std::unique_ptr<lens::ContextualInputData> input_data =
      std::make_unique<lens::ContextualInputData>();
  input_data->primary_content_type = lens::MimeType::kPdf;
  input_data->context_input = std::vector<lens::ContextualInput>();
  input_data->page_url = page_url;
  input_data->page_title = page_title;
  input_data->pdf_current_page = 1;
  input_data->context_input->push_back(
      lens::ContextualInput(std::vector<uint8_t>(), lens::MimeType::kPdf));
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorRED);  // Fill with a solid color
  input_data->viewport_screenshot = bitmap;
  lens::ImageEncodingOptions image_options{.max_size = 1000000,
                                           .max_height = 1000,
                                           .max_width = 1000,
                                           .compression_quality = 30};
  controller().StartFileUploadFlow(file_token, std::move(input_data),
                                   image_options);

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Get the file and viewport upload requests.
  std::optional<lens::LensOverlayServerRequest> file_upload_request;
  std::optional<lens::LensOverlayServerRequest> viewport_upload_request;
  if (controller()
          .recent_sent_upload_request(0)
          ->objects_request()
          .has_image_data()) {
    EXPECT_FALSE(controller()
                     .recent_sent_upload_request(1)
                     ->objects_request()
                     .has_image_data());
    viewport_upload_request = controller().recent_sent_upload_request(0);
    file_upload_request = controller().recent_sent_upload_request(1);
  } else {
    EXPECT_TRUE(controller()
                    .recent_sent_upload_request(1)
                    ->objects_request()
                    .has_image_data());
    file_upload_request = controller().recent_sent_upload_request(0);
    viewport_upload_request = controller().recent_sent_upload_request(1);
  }

  // Validate that the media types specify just the type of context in the
  // individual upload request, instead of PDF_AND_IMAGE.
  EXPECT_EQ(file_upload_request->objects_request()
                .request_context()
                .request_id()
                .media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_PDF);
  EXPECT_EQ(viewport_upload_request->objects_request()
                .request_context()
                .request_id()
                .media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  // Validate that the file upload request and the viewport upload request have
  // different request ids.
  EXPECT_NE(file_upload_request->objects_request()
                .request_context()
                .request_id()
                .uuid(),
            viewport_upload_request->objects_request()
                .request_context()
                .request_id()
                .uuid());

  // Act: Create the destination URL for the query.
  std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info =
      std::make_unique<CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = "hello";
  search_url_request_info->query_start_time = kTestQueryStartTime;
  search_url_request_info->file_tokens.push_back(file_token);
  base::test::TestFuture<GURL> url_future;
  controller().CreateSearchUrl(std::move(search_url_request_info),
                               url_future.GetCallback());
  GURL aim_url = url_future.Take();

  // Check that the contextual inputs param contains the request ids.
  lens::LensOverlayContextualInputs contextual_inputs =
      GetContextualInputsFromUrl(aim_url.spec());
  EXPECT_EQ(contextual_inputs.inputs_size(), 2);

  // The files may be in any order, so find the corresponding request ids.
  bool viewport_contextual_input_is_first_file =
      contextual_inputs.inputs(0).request_id().uuid() ==
      viewport_upload_request->objects_request()
          .request_context()
          .request_id()
          .uuid();
  auto viewport_file_request_id_from_cinpts =
      contextual_inputs.inputs(viewport_contextual_input_is_first_file ? 0 : 1)
          .request_id();
  auto pdf_file_request_id_from_cinpts =
      contextual_inputs.inputs(viewport_contextual_input_is_first_file ? 1 : 0)
          .request_id();

  // Check that the media types are different and match the expected media
  // types.
  EXPECT_EQ(viewport_file_request_id_from_cinpts.media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  EXPECT_EQ(pdf_file_request_id_from_cinpts.media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_PDF);
}

TEST_F(ComposeboxQueryControllerTest, CreateSearchUrlWithInvocationSource) {
  CreateController(/*send_lns_surface=*/false);
  controller().InitializeIfNeeded();
  WaitForClusterInfo();

  std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info =
      std::make_unique<CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = "test query";
  search_url_request_info->query_start_time = kTestQueryStartTime;
  search_url_request_info->invocation_source =
      lens::LensOverlayInvocationSource::kAppMenu;

  base::test::TestFuture<GURL> url_future;
  controller().CreateSearchUrl(std::move(search_url_request_info),
                               url_future.GetCallback());
  GURL aim_url = url_future.Take();

  std::string source_param;
  EXPECT_TRUE(net::GetValueForKeyInQuery(aim_url, "source", &source_param));
  EXPECT_EQ(source_param, "chrome.crn.menu");
}

TEST_F(ComposeboxQueryControllerTest,
       UploadPageContextPdfFileWithViewportRequestSuccess) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow with multiple context inputs and page
  // context params.
  GURL page_url = GURL("https://www.test.com");
  std::string page_title = "Test Page";
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  std::unique_ptr<lens::ContextualInputData> input_data =
      std::make_unique<lens::ContextualInputData>();
  input_data->primary_content_type = lens::MimeType::kPdf;
  input_data->context_input = std::vector<lens::ContextualInput>();
  input_data->page_url = page_url;
  input_data->page_title = page_title;
  input_data->pdf_current_page = 1;
  input_data->context_input->push_back(
      lens::ContextualInput(std::vector<uint8_t>(), lens::MimeType::kPdf));
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorRED);  // Fill with a solid color
  input_data->viewport_screenshot = bitmap;
  lens::ImageEncodingOptions image_options{.max_size = 1000000,
                                           .max_height = 1000,
                                           .max_width = 1000,
                                           .compression_quality = 30};
  controller().StartFileUploadFlow(file_token, std::move(input_data),
                                   image_options);

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Get the file and viewport upload requests.
  std::optional<lens::LensOverlayServerRequest> file_upload_request;
  std::optional<lens::LensOverlayServerRequest> viewport_upload_request;
  if (controller()
          .recent_sent_upload_request(0)
          ->objects_request()
          .has_image_data()) {
    EXPECT_FALSE(controller()
                     .recent_sent_upload_request(1)
                     ->objects_request()
                     .has_image_data());
    viewport_upload_request = controller().recent_sent_upload_request(0);
    file_upload_request = controller().recent_sent_upload_request(1);
  } else {
    EXPECT_TRUE(controller()
                    .recent_sent_upload_request(1)
                    ->objects_request()
                    .has_image_data());
    file_upload_request = controller().recent_sent_upload_request(0);
    viewport_upload_request = controller().recent_sent_upload_request(1);
  }

  // Validate the file upload request payload.
  EXPECT_EQ(file_upload_request->objects_request()
                .payload()
                .content()
                .content_data(0)
                .content_type(),
            lens::ContentData::CONTENT_TYPE_PDF);
  EXPECT_EQ(file_upload_request->objects_request()
                .payload()
                .content()
                .content_data(0)
                .compression_type(),
            kExpectedPdfCompressionType);
  EXPECT_EQ(file_upload_request->objects_request()
                .payload()
                .content()
                .webpage_title(),
            page_title);
  EXPECT_EQ(
      file_upload_request->objects_request().payload().content().webpage_url(),
      page_url.spec());
  // Validate the viewport upload request payload.
  EXPECT_EQ(viewport_upload_request->objects_request()
                .image_data()
                .image_metadata()
                .width(),
            100);
  EXPECT_EQ(viewport_upload_request->objects_request()
                .image_data()
                .image_metadata()
                .height(),
            100);
  EXPECT_EQ(viewport_upload_request->objects_request()
                .viewport_request_context()
                .pdf_page_number(),
            1);
  // Check that the vsrid matches that for a pdf upload with viewport.
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .image_sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .long_context_id(),
            1);
  EXPECT_EQ(file_upload_request->objects_request()
                .request_context()
                .request_id()
                .sequence_id(),
            1);
  EXPECT_EQ(file_upload_request->objects_request()
                .request_context()
                .request_id()
                .image_sequence_id(),
            1);
  EXPECT_EQ(file_upload_request->objects_request()
                .request_context()
                .request_id()
                .long_context_id(),
            1);
  EXPECT_EQ(file_upload_request->objects_request()
                .request_context()
                .request_id()
                .media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_PDF_AND_IMAGE);
  EXPECT_EQ(viewport_upload_request->objects_request()
                .request_context()
                .request_id()
                .sequence_id(),
            1);
  EXPECT_EQ(viewport_upload_request->objects_request()
                .request_context()
                .request_id()
                .image_sequence_id(),
            1);
  EXPECT_EQ(viewport_upload_request->objects_request()
                .request_context()
                .request_id()
                .long_context_id(),
            1);
  EXPECT_EQ(viewport_upload_request->objects_request()
                .request_context()
                .request_id()
                .media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_PDF_AND_IMAGE);
}

TEST_F(ComposeboxQueryControllerTest,
       UploadPageContextWebpageContentWithViewportRequestSuccess) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow with context inputs and page context
  // params.
  GURL page_url = GURL("https://www.test.com");
  std::string page_title = "Test Page";
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  std::unique_ptr<lens::ContextualInputData> input_data =
      std::make_unique<lens::ContextualInputData>();
  input_data->primary_content_type = lens::MimeType::kAnnotatedPageContent;
  input_data->context_input = std::vector<lens::ContextualInput>();
  input_data->page_url = page_url;
  input_data->page_title = page_title;
  input_data->context_input->push_back(lens::ContextualInput(
      std::vector<uint8_t>(), lens::MimeType::kAnnotatedPageContent));
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorRED);  // Fill with a solid color
  input_data->viewport_screenshot = bitmap;
  lens::ImageEncodingOptions image_options{.max_size = 1000000,
                                           .max_height = 1000,
                                           .max_width = 1000,
                                           .compression_quality = 30};
  controller().StartFileUploadFlow(file_token, std::move(input_data),
                                   image_options);

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kAnnotatedPageContent);

  // Act: Create the destination URL for the query. The destination URL can
  // only be created after the cluster info is received.
  std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info =
      std::make_unique<CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = "hello";
  search_url_request_info->query_start_time = kTestQueryStartTime;
  search_url_request_info->file_tokens.push_back(file_token);
  base::test::TestFuture<GURL> url_future;
  controller().CreateSearchUrl(std::move(search_url_request_info),
                               url_future.GetCallback());
  GURL aim_url = url_future.Take();

  // Check that the vsint is populated correctly.
  auto vsint = GetVsintFromUrl(aim_url);
  EXPECT_EQ(vsint.text_select().selected_texts(), "hello");
  EXPECT_TRUE(vsint.log_data().is_parent_query());
  EXPECT_EQ(vsint.interaction_type(),
            lens::LensOverlayInteractionRequestMetadata::WEBPAGE_QUERY);
  EXPECT_TRUE(vsint.has_zoomed_crop());
  EXPECT_EQ(vsint.zoomed_crop().zoom(), 1);
  EXPECT_EQ(vsint.zoomed_crop().crop().coordinate_type(),
            lens::CoordinateType::NORMALIZED);

  // Get the file and viewport upload requests.
  std::optional<lens::LensOverlayServerRequest> file_upload_request;
  std::optional<lens::LensOverlayServerRequest> viewport_upload_request;
  if (controller()
          .recent_sent_upload_request(0)
          ->objects_request()
          .has_image_data()) {
    EXPECT_FALSE(controller()
                     .recent_sent_upload_request(1)
                     ->objects_request()
                     .has_image_data());
    viewport_upload_request = controller().recent_sent_upload_request(0);
    file_upload_request = controller().recent_sent_upload_request(1);
  } else {
    EXPECT_TRUE(controller()
                    .recent_sent_upload_request(1)
                    ->objects_request()
                    .has_image_data());
    file_upload_request = controller().recent_sent_upload_request(0);
    viewport_upload_request = controller().recent_sent_upload_request(1);
  }

  // Validate the file upload request payload.
  EXPECT_EQ(file_upload_request->objects_request()
                .payload()
                .content()
                .content_data(0)
                .content_type(),
            lens::ContentData::CONTENT_TYPE_ANNOTATED_PAGE_CONTENT);
  // Only pdf data is compressed - annotated page content should not be.
  EXPECT_EQ(file_upload_request->objects_request()
                .payload()
                .content()
                .content_data(0)
                .compression_type(),
            lens::CompressionType::UNCOMPRESSED);
  EXPECT_EQ(file_upload_request->objects_request()
                .payload()
                .content()
                .webpage_title(),
            page_title);
  EXPECT_EQ(
      file_upload_request->objects_request().payload().content().webpage_url(),
      page_url.spec());
  // Validate the viewport upload request payload.
  EXPECT_EQ(viewport_upload_request->objects_request()
                .image_data()
                .image_metadata()
                .width(),
            100);
  EXPECT_EQ(viewport_upload_request->objects_request()
                .image_data()
                .image_metadata()
                .height(),
            100);
  // Check that the vsrid matches that for a webpage content upload with
  // viewport.
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .image_sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .long_context_id(),
            1);
  EXPECT_EQ(file_upload_request->objects_request()
                .request_context()
                .request_id()
                .sequence_id(),
            1);
  EXPECT_EQ(file_upload_request->objects_request()
                .request_context()
                .request_id()
                .image_sequence_id(),
            1);
  EXPECT_EQ(file_upload_request->objects_request()
                .request_context()
                .request_id()
                .long_context_id(),
            1);
  EXPECT_EQ(file_upload_request->objects_request()
                .request_context()
                .request_id()
                .media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE);
  EXPECT_EQ(viewport_upload_request->objects_request()
                .request_context()
                .request_id()
                .sequence_id(),
            1);
  EXPECT_EQ(viewport_upload_request->objects_request()
                .request_context()
                .request_id()
                .image_sequence_id(),
            1);
  EXPECT_EQ(viewport_upload_request->objects_request()
                .request_context()
                .request_id()
                .long_context_id(),
            1);
  EXPECT_EQ(viewport_upload_request->objects_request()
                .request_context()
                .request_id()
                .media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE);

  // Check that the Lens request id is in the AIM url is correct.
  std::string vsrid_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(aim_url, kRequestIdParameterKey,
                                         &vsrid_value));
  EXPECT_FALSE(vsrid_value.empty());
  EXPECT_EQ(lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE,
            DecodeRequestIdFromVsrid(vsrid_value).media_type());

  // Assert: Visual input type is set to wp for webpage queries.
  std::string vit_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(aim_url, kVisualInputTypeParameterKey,
                                         &vit_value));
  EXPECT_EQ(vit_value, "wp");
}

TEST_F(ComposeboxQueryControllerTest,
       UploadPageContextPdfFileWithViewportButViewportsDisabledRequestSuccess) {
  // Create the controller with viewports disabled.
  CreateController(/*send_lns_surface=*/false,
                   /*suppress_lns_surface_param_if_no_image=*/true,
                   /*enable_multi_context_input_flow=*/true,
                   /*enable_viewport_images=*/false);

  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow with viewport and pdf context inputs.
  GURL page_url = GURL("https://www.test.com");
  std::string page_title = "Test Page";
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  std::unique_ptr<lens::ContextualInputData> input_data =
      std::make_unique<lens::ContextualInputData>();
  input_data->primary_content_type = lens::MimeType::kPdf;
  input_data->context_input = std::vector<lens::ContextualInput>();
  input_data->page_url = page_url;
  input_data->page_title = page_title;
  input_data->pdf_current_page = 1;
  input_data->context_input->push_back(
      lens::ContextualInput(std::vector<uint8_t>(), lens::MimeType::kPdf));
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorRED);  // Fill with a solid color
  input_data->viewport_screenshot = bitmap;
  lens::ImageEncodingOptions image_options{.max_size = 1000000,
                                           .max_height = 1000,
                                           .max_width = 1000,
                                           .compression_quality = 30};
  controller().StartFileUploadFlow(file_token, std::move(input_data),
                                   image_options);

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Get the file and viewport upload requests.
  std::optional<lens::LensOverlayServerRequest> file_upload_request =
      controller().last_sent_file_upload_request();

  // Validate the file upload request payload.
  EXPECT_EQ(file_upload_request->objects_request()
                .request_context()
                .request_id()
                .media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_PDF);
}
#endif  // !BUILDFLAG(IS_IOS)

TEST_F(ComposeboxQueryControllerTest,
       UploadPageContextWebpageContentWithPageContextIneligibleFailure) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow with context inputs and page context
  // params.
  GURL page_url = GURL("https://www.test.com");
  std::string page_title = "Test Page";
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  std::unique_ptr<lens::ContextualInputData> input_data =
      std::make_unique<lens::ContextualInputData>();
  input_data->primary_content_type = lens::MimeType::kAnnotatedPageContent;
  input_data->context_input = std::vector<lens::ContextualInput>();
  input_data->page_url = page_url;
  input_data->page_title = page_title;
  input_data->context_input->push_back(lens::ContextualInput(
      std::vector<uint8_t>(), lens::MimeType::kAnnotatedPageContent));
  input_data->is_page_context_eligible = false;
  controller().StartFileUploadFlow(file_token, std::move(input_data),
                                   std::nullopt);

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kAnnotatedPageContent,
                    FileUploadStatus::kValidationFailed,
                    FileUploadErrorType::kBrowserProcessingError);
}

TEST_F(ComposeboxQueryControllerTest, UploadInvalidMimeTypeFileRequestFailure) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();

  lens::MimeType mime_type = lens::MimeType::kJson;
  std::unique_ptr<lens::ContextualInputData> input_data =
      std::make_unique<lens::ContextualInputData>();
  input_data->primary_content_type = mime_type;

  controller().StartFileUploadFlow(file_token, std::move(input_data),
                                   /*image_options=*/std::nullopt);

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, mime_type, FileUploadStatus::kValidationFailed,
                    FileUploadErrorType::kBrowserProcessingError);
}

TEST_F(ComposeboxQueryControllerTest, UploadUnknownMimeTypeFileRequestSuccess) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  std::unique_ptr<lens::ContextualInputData> input_data =
      std::make_unique<lens::ContextualInputData>();
  input_data->primary_content_type = lens::MimeType::kUnknown;
  input_data->context_input = std::vector<lens::ContextualInput>();
  input_data->page_title = "Title";
  input_data->page_url = GURL("https://www.example.com");

  controller().StartFileUploadFlow(file_token, std::move(input_data),
                                   /*image_options=*/std::nullopt);

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kUnknown);

  // Validate the file upload request payload.
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .payload()
                .content()
                .webpage_title(),
            "Title");
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .payload()
                .content()
                .webpage_url(),
            GURL("https://www.example.com"));
  EXPECT_EQ(controller()
                .last_sent_file_upload_request()
                ->objects_request()
                .payload()
                .content()
                .content_data_size(),
            0);
}

TEST_F(ComposeboxQueryControllerTest, UploadFileRequestSuccessWithOAuth) {
  // Arrange: Make primary account available.
  identity_test_env()->MakePrimaryAccountAvailable(
      kTestUser, signin::ConsentLevel::kSignin);

  // Act: Start the session.
  controller().InitializeIfNeeded();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>());
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);
}

TEST_F(ComposeboxQueryControllerTest, UploadFileAndWaitForClusterInfoExpire) {
  // Enable cluster info TTL.
  controller().set_enable_cluster_info_ttl(true);

  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Wait 1 hour.
  task_environment().FastForwardBy(base::Hours(1));

  // Assert: Validate file upload request and status changes.

  FileUploadStatusTuple expired_file_upload_status =
      file_upload_status_future_.Take();
  EXPECT_EQ(file_token, std::get<0>(expired_file_upload_status));
  EXPECT_EQ(lens::MimeType::kPdf, std::get<1>(expired_file_upload_status));
  EXPECT_EQ(FileUploadStatus::kUploadExpired,
            std::get<2>(expired_file_upload_status));
  EXPECT_EQ(std::nullopt, std::get<3>(expired_file_upload_status));
}

TEST_F(ComposeboxQueryControllerTest,
       UploadFileRequestWithOAuthAndDelayedClusterInfo) {
  // Arrange: Make primary account available.
  identity_test_env()->MakePrimaryAccountAvailable(
      kTestUser, signin::ConsentLevel::kSignin);
  // Arrange: Listen for the controller state changes.
  base::test::TestFuture<QueryControllerState> controller_state_future;
  controller().set_on_query_controller_state_changed_callback(
      controller_state_future.GetRepeatingCallback());

  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Act: Start the file upload flow without waiting for the cluster info
  // request to complete.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>());

  // Assert: Validate file upload status change.
  FileUploadStatusTuple processing_file_upload_status =
      file_upload_status_future_.Take();
  EXPECT_EQ(file_token, std::get<0>(processing_file_upload_status));
  EXPECT_EQ(lens::MimeType::kPdf, std::get<1>(processing_file_upload_status));
  EXPECT_EQ(FileUploadStatus::kProcessing,
            std::get<2>(processing_file_upload_status));
  EXPECT_EQ(std::nullopt, std::get<3>(processing_file_upload_status));

  // Act: Send the oauth token for the cluster info or file upload request.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);

  // Act: Send the oauth token for the other request.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);

  // Assert: Validate cluster info request and state changes.
  EXPECT_EQ(QueryControllerState::kAwaitingClusterInfoResponse,
            controller_state_future.Take());
  EXPECT_EQ(QueryControllerState::kAwaitingClusterInfoResponse,
            controller().query_controller_state());

  EXPECT_EQ(QueryControllerState::kClusterInfoReceived,
            controller_state_future.Take());
  EXPECT_EQ(QueryControllerState::kClusterInfoReceived,
            controller().query_controller_state());

  EXPECT_EQ(controller().num_cluster_info_fetch_requests_sent(), 1);

  // Assert: Validate file status changes now that cluster info is received.
  FileUploadStatusTuple suggest_ready_file_upload_status =
      file_upload_status_future_.Take();
  EXPECT_EQ(file_token, std::get<0>(suggest_ready_file_upload_status));
  EXPECT_EQ(lens::MimeType::kPdf,
            std::get<1>(suggest_ready_file_upload_status));
  EXPECT_EQ(FileUploadStatus::kProcessingSuggestSignalsReady,
            std::get<2>(suggest_ready_file_upload_status));
  EXPECT_EQ(std::nullopt, std::get<3>(suggest_ready_file_upload_status));

  // Assert: Validate file upload request and status changes.
  FileUploadStatusTuple upload_started_file_upload_status =
      file_upload_status_future_.Take();
  EXPECT_EQ(file_token, std::get<0>(upload_started_file_upload_status));
  EXPECT_EQ(lens::MimeType::kPdf,
            std::get<1>(upload_started_file_upload_status));
  EXPECT_EQ(FileUploadStatus::kUploadStarted,
            std::get<2>(upload_started_file_upload_status));
  EXPECT_EQ(std::nullopt, std::get<3>(upload_started_file_upload_status));

  FileUploadStatusTuple upload_successful_file_upload_status =
      file_upload_status_future_.Take();
  EXPECT_EQ(file_token, std::get<0>(upload_successful_file_upload_status));
  EXPECT_EQ(lens::MimeType::kPdf,
            std::get<1>(upload_successful_file_upload_status));
  EXPECT_EQ(FileUploadStatus::kUploadSuccessful,
            std::get<2>(upload_successful_file_upload_status));
  EXPECT_EQ(std::nullopt, std::get<3>(upload_successful_file_upload_status));

  EXPECT_EQ(controller().num_file_upload_requests_sent(), 1);
  EXPECT_THAT(GetGsessionIdFromUrl(controller().last_sent_fetch_url()),
              testing::Optional(std::string(kTestServerSessionId)));
}

TEST_F(ComposeboxQueryControllerTest, CreateClientContextHasCorrectValues) {
  // Act: Get the client context.
  lens::LensOverlayClientContext client_context = controller().client_context();

  // Assert: Validate the client context values.
  EXPECT_EQ(client_context.surface(), lens::SURFACE_LENS_OVERLAY);
  EXPECT_EQ(client_context.platform(), lens::PLATFORM_LENS_OVERLAY);
  EXPECT_EQ(client_context.locale_context().language(), kLocale);
  EXPECT_EQ(client_context.locale_context().region(), kRegion);
  EXPECT_EQ(client_context.locale_context().time_zone(), kTimeZone);
}

TEST_F(ComposeboxQueryControllerTest,
       CreateSearchUrlRequestQueuedUntilClusterInfoReceived) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Verify the controller is in the awaiting state.
  EXPECT_EQ(QueryControllerState::kAwaitingClusterInfoResponse,
            controller().query_controller_state());

  // Act: Start the file upload flow to ensure we attempt a multimodal request.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>());

  // Act: Generate the destination URL for the query.
  std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info =
      std::make_unique<CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = "test";
  search_url_request_info->query_start_time = kTestQueryStartTime;
  search_url_request_info->file_tokens.push_back(file_token);
  base::test::TestFuture<GURL> url_future;
  controller().CreateSearchUrl(std::move(search_url_request_info),
                               url_future.GetCallback());

  // Assert: The callback has not been run yet because the cluster info is
  // pending.
  EXPECT_FALSE(url_future.IsReady());

  // Act: Wait for the cluster info response.
  WaitForClusterInfo();

  // Assert: The callback has been run now that the cluster info has been
  // received.
  ASSERT_TRUE(url_future.Wait());
  GURL aim_url = url_future.Take();

  // Check that the timestamps are attached to the url to verify the request was
  // processed.
  std::string qsubts_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      aim_url, kQuerySubmissionTimeQueryParameter, &qsubts_value));

  // Verify other expected parameters to ensure it's a valid AIM URL.
  std::string udm_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(aim_url, kSearchModeQueryParameterKey,
                                         &udm_value));
  EXPECT_EQ(udm_value, kAimUdmQueryParameterValue);
}

TEST_F(ComposeboxQueryControllerTest,
       CreateSearchUrlWithInvalidClusterInfoReturnsAimUrl) {
  // Arrange: Simulate an error in the cluster info request.
  controller().set_next_cluster_info_request_should_return_error(true);

  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Verify the controller is in the awaiting state.
  EXPECT_EQ(QueryControllerState::kAwaitingClusterInfoResponse,
            controller().query_controller_state());

  // Act: Start the file upload flow to ensure we attempt a multimodal request.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>());

  // Act: Generate the destination URL for the query.
  std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info =
      std::make_unique<CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = "test";
  search_url_request_info->query_start_time = kTestQueryStartTime;
  search_url_request_info->file_tokens.push_back(file_token);
  base::test::TestFuture<GURL> url_future;
  controller().CreateSearchUrl(std::move(search_url_request_info),
                               url_future.GetCallback());

  // Assert: The callback has not been run yet because the cluster info is
  // pending.
  EXPECT_FALSE(url_future.IsReady());

  // Act: Wait for the cluster info response (which will fail).
  WaitForClusterInfo(QueryControllerState::kClusterInfoInvalid);

  // Assert: The callback should have been run with an AIM URL.
  ASSERT_TRUE(url_future.Wait());
  GURL aim_url = url_future.Take();
  EXPECT_FALSE(aim_url.is_empty());

  // Check that the udm parameter is set to 50 (AIM).
  std::string udm_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(aim_url, kSearchModeQueryParameterKey,
                                         &udm_value));
  EXPECT_EQ(udm_value, kAimUdmQueryParameterValue);
}

TEST_F(
    ComposeboxQueryControllerTest,
    CreateSearchUrlForTextOnlyQueryWhileAwaitingClusterInfoReturnsImmediately) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Verify the controller is in the awaiting state.
  EXPECT_EQ(QueryControllerState::kAwaitingClusterInfoResponse,
            controller().query_controller_state());

  // Act: Generate the destination URL for the query.
  std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info =
      std::make_unique<CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = "test";
  search_url_request_info->query_start_time = kTestQueryStartTime;
  base::test::TestFuture<GURL> url_future;
  controller().CreateSearchUrl(std::move(search_url_request_info),
                               url_future.GetCallback());

  // Assert: The callback has been run immediately because text-only queries are
  // not queued.
  EXPECT_TRUE(url_future.IsReady());
  GURL aim_url = url_future.Take();

  // Check that the timestamps are attached to the url to verify the request was
  // processed.
  std::string qsubts_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      aim_url, kQuerySubmissionTimeQueryParameter, &qsubts_value));

  // Verify other expected parameters to ensure it's a valid AIM URL.
  std::string udm_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(aim_url, kSearchModeQueryParameterKey,
                                         &udm_value));
  EXPECT_EQ(udm_value, kAimUdmQueryParameterValue);
}

TEST_F(ComposeboxQueryControllerTest,
       UnimodalTextQuerySubmittedWithInvalidClusterInfoSuccess) {
  controller().set_next_cluster_info_request_should_return_error(true);

  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo(QueryControllerState::kClusterInfoInvalid);

  // Act: Generate the destination URL for the query.
  std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info =
      std::make_unique<CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = "test";
  search_url_request_info->query_start_time = kTestQueryStartTime;
  base::test::TestFuture<GURL> url_future;
  controller().CreateSearchUrl(std::move(search_url_request_info),
                               url_future.GetCallback());
  GURL aim_url = url_future.Take();

  // Assert: Lens request id is NOT added to unimodal text queries.
  std::string vsrid_value;
  EXPECT_FALSE(net::GetValueForKeyInQuery(aim_url, kRequestIdParameterKey,
                                          &vsrid_value));

  // Assert: Visual input type is NOT added to unimodal text queries.
  std::string vit_value;
  EXPECT_FALSE(net::GetValueForKeyInQuery(aim_url, kVisualInputTypeParameterKey,
                                          &vit_value));

  // Assert: Gsession id is NOT added to unimodal text queries.
  std::string gsession_id_value;
  EXPECT_FALSE(net::GetValueForKeyInQuery(aim_url, kSessionIdQueryParameterKey,
                                          &gsession_id_value));

  // Check that the timestamps are attached to the url.
  std::string qsubts_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      aim_url, kQuerySubmissionTimeQueryParameter, &qsubts_value));

  std::string cud_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      aim_url, kClientUploadDurationQueryParameter, &cud_value));
}

TEST_F(ComposeboxQueryControllerTest, QuerySubmitted) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Generate the destination URL for the query.
  std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info =
      std::make_unique<CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = "test";
  search_url_request_info->query_start_time = kTestQueryStartTime;
  base::test::TestFuture<GURL> url_future;
  controller().CreateSearchUrl(std::move(search_url_request_info),
                               url_future.GetCallback());
  GURL aim_url = url_future.Take();

  // Assert: Lens request id is NOT added to unimodal text queries.
  std::string vsrid_value;
  EXPECT_FALSE(net::GetValueForKeyInQuery(aim_url, kRequestIdParameterKey,
                                          &vsrid_value));

  // Assert: Visual input type is NOT added to unimodal text queries.
  std::string vit_value;
  EXPECT_FALSE(net::GetValueForKeyInQuery(aim_url, kVisualInputTypeParameterKey,
                                          &vit_value));

  // Assert: Gsession id is NOT added to unimodal text queries.
  std::string gsession_id_value;
  EXPECT_FALSE(net::GetValueForKeyInQuery(aim_url, kSessionIdQueryParameterKey,
                                          &gsession_id_value));

  // Check that the timestamps are attached to the url.
  std::string qsubts_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      aim_url, kQuerySubmissionTimeQueryParameter, &qsubts_value));

  std::string cud_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      aim_url, kClientUploadDurationQueryParameter, &cud_value));

  // Check that the udm parameter is set to 50 (AIM).
  std::string udm_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(aim_url, kSearchModeQueryParameterKey,
                                         &udm_value));
  EXPECT_EQ(udm_value, kAimUdmQueryParameterValue);
}

TEST_F(ComposeboxQueryControllerTest, QuerySubmittedWithUploadedPdf) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Act: Create the destination URL for the query. The destination URL can
  // only be created after the cluster info is received.
  std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info =
      std::make_unique<CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = "hello";
  search_url_request_info->query_start_time = kTestQueryStartTime;
  search_url_request_info->file_tokens.push_back(file_token);
  base::test::TestFuture<GURL> url_future;
  controller().CreateSearchUrl(std::move(search_url_request_info),
                               url_future.GetCallback());
  GURL aim_url = url_future.Take();

  // Check that the vsint is populated correctly.
  auto vsint = GetVsintFromUrl(aim_url);
  EXPECT_EQ(vsint.text_select().selected_texts(), "hello");
  EXPECT_TRUE(vsint.log_data().is_parent_query());
  EXPECT_EQ(vsint.interaction_type(),
            lens::LensOverlayInteractionRequestMetadata::PDF_QUERY);
  EXPECT_FALSE(vsint.has_zoomed_crop());

  // Assert: Lens request id is NOT added to multimodal pdf queries.
  std::string vsrid_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(aim_url, kRequestIdParameterKey,
                                         &vsrid_value));
  EXPECT_FALSE(vsrid_value.empty());
  EXPECT_EQ(lens::LensOverlayRequestId::MEDIA_TYPE_PDF,
            DecodeRequestIdFromVsrid(vsrid_value).media_type());

  // Assert: Visual input type is set to pdf for multimodal pdf queries.
  std::string vit_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(aim_url, kVisualInputTypeParameterKey,
                                         &vit_value));
  EXPECT_EQ(vit_value, "pdf");

  // Assert: Gsession id is added to multimodal pdf queries.
  std::string gsession_id_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(aim_url, kSessionIdQueryParameterKey,
                                         &gsession_id_value));
  EXPECT_EQ(kTestSearchSessionId, gsession_id_value);

  // Check that the timestamps are attached to the url.
  std::string qsubts_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      aim_url, kQuerySubmissionTimeQueryParameter, &qsubts_value));

  std::string cud_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      aim_url, kClientUploadDurationQueryParameter, &cud_value));

  // Check that the udm value is set to 50 (AIM).
  std::string udm_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(aim_url, kSearchModeQueryParameterKey,
                                         &udm_value));
  EXPECT_EQ(udm_value, kAimUdmQueryParameterValue);
}

TEST_F(ComposeboxQueryControllerTest, CreateClientToAimRequestWithUploadedPdf) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Act: Create the ClientToAimRequest.
  std::unique_ptr<CreateClientToAimRequestInfo> client_to_aim_request_info =
      std::make_unique<CreateClientToAimRequestInfo>();
  client_to_aim_request_info->query_text = "hello";
  client_to_aim_request_info->query_text_source =
      lens::QueryPayload::QUERY_TEXT_SOURCE_KEYBOARD_INPUT;
  client_to_aim_request_info->file_tokens.push_back(file_token);
  client_to_aim_request_info->query_start_time = kTestQueryStartTime;
  std::optional<lens::ClientToAimMessage> client_to_aim_request =
      controller().CreateClientToAimRequest(
          std::move(client_to_aim_request_info));

  // Assert: The ClientToAimRequest is populated correctly.
  ASSERT_TRUE(client_to_aim_request.has_value());
  ASSERT_EQ(client_to_aim_request->submit_query()
                .payload()
                .lens_image_query_data(0)
                .request_id()
                .media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_PDF);
  EXPECT_EQ(client_to_aim_request->submit_query().payload().query_text(),
            "hello");
  EXPECT_EQ(client_to_aim_request->submit_query().payload().query_text_source(),
            lens::QueryPayload::QUERY_TEXT_SOURCE_KEYBOARD_INPUT);
  EXPECT_EQ(client_to_aim_request->submit_query()
                .payload()
                .lens_image_query_data(0)
                .search_session_id(),
            kTestSearchSessionId);
}

TEST_F(ComposeboxQueryControllerTest,
       CreateClientToAimRequestWithAdditionalCgiParams) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Create the ClientToAimRequest.
  std::unique_ptr<CreateClientToAimRequestInfo> client_to_aim_request_info =
      std::make_unique<CreateClientToAimRequestInfo>();
  client_to_aim_request_info->query_text = "hello";
  client_to_aim_request_info->query_text_source =
      lens::QueryPayload::QUERY_TEXT_SOURCE_KEYBOARD_INPUT;
  client_to_aim_request_info->additional_cgi_params["key1"] = "value1";
  client_to_aim_request_info->additional_cgi_params["key2"] = "value2";

  std::optional<lens::ClientToAimMessage> client_to_aim_request =
      controller().CreateClientToAimRequest(
          std::move(client_to_aim_request_info));

  // Assert: The ClientToAimRequest is populated correctly.
  ASSERT_TRUE(client_to_aim_request.has_value());
  EXPECT_EQ(client_to_aim_request->submit_query().payload().query_text(),
            "hello");
  EXPECT_EQ(client_to_aim_request->submit_query().payload().query_text_source(),
            lens::QueryPayload::QUERY_TEXT_SOURCE_KEYBOARD_INPUT);
  const auto& params =
      client_to_aim_request->submit_query().payload().additional_cgi_params();
  EXPECT_EQ(params.size(), 2u);
  EXPECT_EQ(params.at("key1"), "value1");
  EXPECT_EQ(params.at("key2"), "value2");
}

TEST_F(ComposeboxQueryControllerTest,
       QuerySubmittedWithUploadedPdfStandardSearch) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Act: Create the destination URL for the query. The destination URL can
  // only be created after the cluster info is received.
  std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info =
      std::make_unique<CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = "hello";
  search_url_request_info->search_url_type =
      ComposeboxQueryController::SearchUrlType::kStandard;
  search_url_request_info->file_tokens.push_back(file_token);
  search_url_request_info->query_start_time = kTestQueryStartTime;
  base::test::TestFuture<GURL> url_future;
  controller().CreateSearchUrl(std::move(search_url_request_info),
                               url_future.GetCallback());
  GURL search_url = url_future.Take();

  // Check that the vsint is populated correctly.
  auto vsint = GetVsintFromUrl(search_url);
  EXPECT_EQ(vsint.text_select().selected_texts(), "hello");
  EXPECT_TRUE(vsint.log_data().is_parent_query());
  EXPECT_EQ(vsint.interaction_type(),
            lens::LensOverlayInteractionRequestMetadata::PDF_QUERY);
  EXPECT_FALSE(vsint.has_zoomed_crop());

  // Assert: Lens request id is added to multimodal pdf queries.
  std::string vsrid_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(search_url, kRequestIdParameterKey,
                                         &vsrid_value));
  EXPECT_FALSE(vsrid_value.empty());
  EXPECT_EQ(lens::LensOverlayRequestId::MEDIA_TYPE_PDF,
            DecodeRequestIdFromVsrid(vsrid_value).media_type());

  // Assert: Visual input type is set to pdf for multimodal pdf queries.
  std::string vit_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      search_url, kVisualInputTypeParameterKey, &vit_value));
  EXPECT_EQ(vit_value, "pdf");

  // Assert: Gsession id is added to multimodal pdf queries.
  std::string gsession_id_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      search_url, kSessionIdQueryParameterKey, &gsession_id_value));
  EXPECT_EQ(kTestSearchSessionId, gsession_id_value);

  // Check that the timestamps are attached to the url.
  std::string qsubts_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      search_url, kQuerySubmissionTimeQueryParameter, &qsubts_value));

  std::string cud_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      search_url, kClientUploadDurationQueryParameter, &cud_value));

  // Check that the udm value is set to 24 (multimodal search).
  std::string udm_value_24;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      search_url, kSearchModeQueryParameterKey, &udm_value_24));
  EXPECT_EQ(udm_value_24, kMultimodalUdmQueryParameterValue);

  // Act: Create the destination URL for the query, with no query text.
  std::unique_ptr<CreateSearchUrlRequestInfo>
      search_url_request_info_no_query_text =
          std::make_unique<CreateSearchUrlRequestInfo>();
  search_url_request_info_no_query_text->search_url_type =
      ComposeboxQueryController::SearchUrlType::kStandard;
  search_url_request_info_no_query_text->query_start_time = kTestQueryStartTime;
  search_url_request_info_no_query_text->file_tokens.push_back(file_token);
  base::test::TestFuture<GURL> url_future_2;
  controller().CreateSearchUrl(std::move(search_url_request_info_no_query_text),
                               url_future_2.GetCallback());
  GURL no_query_text_url = url_future_2.Take();

  // Check that the vsint is populated correctly.
  auto vsint_2 = GetVsintFromUrl(no_query_text_url);
  EXPECT_EQ(vsint_2.text_select().selected_texts(), "");
  EXPECT_TRUE(vsint_2.log_data().is_parent_query());
  EXPECT_EQ(vsint_2.interaction_type(),
            lens::LensOverlayInteractionRequestMetadata::PDF_QUERY);
  EXPECT_FALSE(vsint_2.has_zoomed_crop());

  // Check that the udm value is set to 26 (unimodal search).
  std::string udm_value_26;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      no_query_text_url, kSearchModeQueryParameterKey, &udm_value_26));
  EXPECT_EQ(udm_value_26, kUnimodalUdmQueryParameterValue);
}

TEST_F(ComposeboxQueryControllerTest,
       InteractionQuerySubmittedWithUploadedPdf) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Act: Create the destination URL for the query. The destination URL can
  // only be created after the cluster info is received.
  std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info =
      std::make_unique<CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = "hello";
  search_url_request_info->search_url_type =
      ComposeboxQueryController::SearchUrlType::kStandard;
  search_url_request_info->query_start_time = kTestQueryStartTime;
  search_url_request_info->lens_overlay_selection_type =
      lens::LensOverlaySelectionType::REGION_SEARCH;
  search_url_request_info->image_crop = lens::ImageCrop();
  search_url_request_info->image_crop->mutable_zoomed_crop()
      ->mutable_crop()
      ->set_coordinate_type(lens::CoordinateType::NORMALIZED);
  search_url_request_info->image_crop->mutable_zoomed_crop()->set_zoom(1);
  search_url_request_info->image_crop->mutable_zoomed_crop()->set_parent_height(
      25);
  search_url_request_info->file_tokens.push_back(file_token);

  search_url_request_info->client_logs = lens::LensOverlayClientLogs();

  // Runloop for when the interaction request is sent.
  base::RunLoop run_loop;
  controller().AddEndpointFetcherCreatedCallback(
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));

  base::test::TestFuture<GURL> url_future;
  controller().CreateSearchUrl(std::move(search_url_request_info),
                               url_future.GetCallback());
  GURL search_url = url_future.Take();

  // Check that an interaction request was created.
  run_loop.Run();
  auto interaction_request = controller().last_sent_interaction_request();
  ASSERT_TRUE(interaction_request.has_value());
  EXPECT_TRUE(interaction_request->has_interaction_request());

  // Check that the vsint is populated correctly.
  auto vsint = GetVsintFromUrl(search_url);
  EXPECT_EQ(vsint.text_select().selected_texts(), "hello");
  EXPECT_TRUE(vsint.log_data().is_parent_query());
  EXPECT_EQ(vsint.interaction_type(),
            lens::LensOverlayInteractionRequestMetadata::REGION_SEARCH);
  EXPECT_TRUE(vsint.has_zoomed_crop());
  EXPECT_EQ(vsint.zoomed_crop().zoom(), 1);
  EXPECT_EQ(vsint.zoomed_crop().parent_height(), 25);
  EXPECT_EQ(vsint.zoomed_crop().crop().coordinate_type(),
            lens::CoordinateType::NORMALIZED);

  // Assert: Lens request id is added to multimodal pdf queries.
  std::string vsrid_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(search_url, kRequestIdParameterKey,
                                         &vsrid_value));
  EXPECT_FALSE(vsrid_value.empty());
  EXPECT_EQ(lens::LensOverlayRequestId::MEDIA_TYPE_PDF,
            DecodeRequestIdFromVsrid(vsrid_value).media_type());

  // Assert: Visual input type is set to pdf for multimodal pdf queries.
  std::string vit_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      search_url, kVisualInputTypeParameterKey, &vit_value));
  EXPECT_EQ(vit_value, "pdf");

  // Assert: Gsession id is added to multimodal pdf queries.
  std::string gsession_id_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      search_url, kSessionIdQueryParameterKey, &gsession_id_value));
  EXPECT_EQ(kTestSearchSessionId, gsession_id_value);

  // Check that the timestamps are attached to the url.
  std::string qsubts_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      search_url, kQuerySubmissionTimeQueryParameter, &qsubts_value));

  std::string cud_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      search_url, kClientUploadDurationQueryParameter, &cud_value));

  // Check that the udm value is set to 24 (multimodal search).
  std::string udm_value_24;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      search_url, kSearchModeQueryParameterKey, &udm_value_24));
  EXPECT_EQ(udm_value_24, kMultimodalUdmQueryParameterValue);
}

TEST_F(ComposeboxQueryControllerTest,
       InteractionQuerySubmittedWithMultiContextFlow) {
  // Act: Create controller with multi-context flow enabled.
  CreateController(/*send_lns_surface=*/false,
                   /*suppress_lns_surface_param_if_no_image=*/true,
                   /*enable_multi_context_input_flow=*/true);

  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Act: Create the destination URL for the query.
  std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info =
      std::make_unique<CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = "hello";
  search_url_request_info->search_url_type =
      ComposeboxQueryController::SearchUrlType::kStandard;
  search_url_request_info->query_start_time = kTestQueryStartTime;
  search_url_request_info->lens_overlay_selection_type =
      lens::LensOverlaySelectionType::REGION_SEARCH;
  search_url_request_info->image_crop = lens::ImageCrop();
  search_url_request_info->image_crop->mutable_zoomed_crop()
      ->mutable_crop()
      ->set_coordinate_type(lens::CoordinateType::NORMALIZED);
  search_url_request_info->image_crop->mutable_zoomed_crop()->set_zoom(1);
  search_url_request_info->image_crop->mutable_zoomed_crop()->set_parent_height(
      25);
  search_url_request_info->file_tokens.push_back(file_token);

  search_url_request_info->client_logs = lens::LensOverlayClientLogs();

  // Runloop for when the interaction request is sent.
  base::RunLoop run_loop;
  controller().AddEndpointFetcherCreatedCallback(
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));

  base::test::TestFuture<GURL> url_future;
  controller().CreateSearchUrl(std::move(search_url_request_info),
                               url_future.GetCallback());
  GURL search_url = url_future.Take();

  // Check that an interaction request was created.
  run_loop.Run();
  auto interaction_request = controller().last_sent_interaction_request();
  ASSERT_TRUE(interaction_request.has_value());
  EXPECT_TRUE(interaction_request->has_interaction_request());

  // Check that the vsint is populated correctly.
  auto vsint = GetVsintFromUrl(search_url);
  EXPECT_EQ(vsint.text_select().selected_texts(), "hello");
  EXPECT_TRUE(vsint.log_data().is_parent_query());
  EXPECT_EQ(vsint.interaction_type(),
            lens::LensOverlayInteractionRequestMetadata::REGION_SEARCH);
  EXPECT_TRUE(vsint.has_zoomed_crop());
  EXPECT_EQ(vsint.zoomed_crop().zoom(), 1);
  EXPECT_EQ(vsint.zoomed_crop().parent_height(), 25);
  EXPECT_EQ(vsint.zoomed_crop().crop().coordinate_type(),
            lens::CoordinateType::NORMALIZED);

  // Assert: Lens request id is added to queries using multi-context flow IF
  // interaction is present.
  std::string vsrid_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(search_url, kRequestIdParameterKey,
                                         &vsrid_value));
  EXPECT_FALSE(vsrid_value.empty());
  EXPECT_EQ(lens::LensOverlayRequestId::MEDIA_TYPE_PDF,
            DecodeRequestIdFromVsrid(vsrid_value).media_type());

  // Assert: Visual input type is NOT added for multi-context flow.
  std::string vit_value;
  EXPECT_FALSE(net::GetValueForKeyInQuery(
      search_url, kVisualInputTypeParameterKey, &vit_value));

  // Assert: Gsession id is added to multimodal queries.
  std::string gsession_id_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      search_url, kSessionIdQueryParameterKey, &gsession_id_value));
  EXPECT_EQ(kTestSearchSessionId, gsession_id_value);

  // Check that the timestamps are attached to the url.
  std::string qsubts_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      search_url, kQuerySubmissionTimeQueryParameter, &qsubts_value));

  std::string cud_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      search_url, kClientUploadDurationQueryParameter, &cud_value));

  // Check that the udm value is set to 24 (multimodal search).
  std::string udm_value_24;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      search_url, kSearchModeQueryParameterKey, &udm_value_24));
  EXPECT_EQ(udm_value_24, kMultimodalUdmQueryParameterValue);

  // Check that the contextual inputs param contains the request ids.
  lens::LensOverlayContextualInputs contextual_inputs =
      GetContextualInputsFromUrl(search_url.spec());
  EXPECT_EQ(contextual_inputs.inputs_size(), 2);
  EXPECT_THAT(contextual_inputs.inputs(1).request_id(),
              EqualsProto(interaction_request->interaction_request()
                              .request_context()
                              .request_id()));
}

#if !BUILDFLAG(IS_IOS)
TEST_F(ComposeboxQueryControllerTest, QuerySubmittedWithUploadedImage) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  std::vector<uint8_t> image_bytes = CreateJPGBytes(100, 100);
  lens::ImageEncodingOptions image_options{.max_size = 1000000,
                                           .max_height = 1000,
                                           .max_width = 1000,
                                           .compression_quality = 30};
  StartImageFileUploadFlow(file_token, image_bytes, image_options);

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kImage);

  // Act: Create the destination URL for the query. The destination URL can
  // only be created after the cluster info is received.
  std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info =
      std::make_unique<CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = "hello";
  search_url_request_info->query_start_time = kTestQueryStartTime;
  search_url_request_info->file_tokens.push_back(file_token);
  base::test::TestFuture<GURL> url_future;
  controller().CreateSearchUrl(std::move(search_url_request_info),
                               url_future.GetCallback());
  GURL aim_url = url_future.Take();

  // Check that the vsint is populated correctly.
  auto vsint = GetVsintFromUrl(aim_url);
  EXPECT_EQ(vsint.text_select().selected_texts(), "hello");
  EXPECT_TRUE(vsint.log_data().is_parent_query());
  EXPECT_EQ(vsint.interaction_type(),
            lens::LensOverlayInteractionRequestMetadata::REGION);
  EXPECT_TRUE(vsint.has_zoomed_crop());
  EXPECT_EQ(vsint.zoomed_crop().zoom(), 1);
  EXPECT_EQ(vsint.zoomed_crop().crop().coordinate_type(),
            lens::CoordinateType::NORMALIZED);

  // Assert: Lens request id is NOT added to multimodal pdf queries.
  std::string vsrid_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(aim_url, kRequestIdParameterKey,
                                         &vsrid_value));
  EXPECT_FALSE(vsrid_value.empty());
  EXPECT_EQ(lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE,
            DecodeRequestIdFromVsrid(vsrid_value).media_type());

  // Assert: Visual input type is set to img for multimodal image queries.
  std::string vit_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(aim_url, kVisualInputTypeParameterKey,
                                         &vit_value));
  EXPECT_EQ(vit_value, "img");

  // Assert: Gsession id is added to multimodal pdf queries.
  std::string gsession_id_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(aim_url, kSessionIdQueryParameterKey,
                                         &gsession_id_value));
  EXPECT_EQ(kTestSearchSessionId, gsession_id_value);

  // Check that the timestamps are attached to the url.
  std::string qsubts_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      aim_url, kQuerySubmissionTimeQueryParameter, &qsubts_value));

  std::string cud_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      aim_url, kClientUploadDurationQueryParameter, &cud_value));
}
#endif  // !BUILDFLAG(IS_IOS)

// TODO(crbug.com/457765080): De-flake and re-enable on iOS.
#if BUILDFLAG(IS_IOS)
#define MAYBE_QuerySubmittedWithUploadedPdfButInvalidClusterInfoIsUnimodal \
  DISABLED_QuerySubmittedWithUploadedPdfButInvalidClusterInfoIsUnimodal
#else
#define MAYBE_QuerySubmittedWithUploadedPdfButInvalidClusterInfoIsUnimodal \
  QuerySubmittedWithUploadedPdfButInvalidClusterInfoIsUnimodal
#endif
TEST_F(ComposeboxQueryControllerTest,
       MAYBE_QuerySubmittedWithUploadedPdfButInvalidClusterInfoIsUnimodal) {
  // Arrange: Create the controller with cluster info TTL enabled.
  CreateController(
      /*send_lns_surface=*/true,
      /*suppress_lns_surface_param_if_no_image=*/true,
      /*enable_multi_context_input_flow=*/false,
      /*enable_viewport_images=*/true,
      /*use_separate_request_ids_for_multi_context_viewport_images=*/true,
      /*enable_cluster_info_ttl=*/true);

  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Ensure that future cluster info requests fail.
  controller().set_next_cluster_info_request_should_return_error(true);

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Wait 1 hour.
  task_environment().FastForwardBy(base::Hours(1));

  // Assert: Validate cluster info request and state changes.
  EXPECT_EQ(QueryControllerState::kClusterInfoInvalid,
            controller().query_controller_state());

  // Act: Create the destination URL for the query.
  std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info =
      std::make_unique<CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = "hello";
  search_url_request_info->query_start_time = kTestQueryStartTime;
  search_url_request_info->file_tokens.push_back(file_token);
  base::test::TestFuture<GURL> url_future;
  controller().CreateSearchUrl(std::move(search_url_request_info),
                               url_future.GetCallback());
  GURL aim_url = url_future.Take();

  // Assert: Lens request id is NOT added to unimodal text queries.
  std::string vsrid_value;
  EXPECT_FALSE(net::GetValueForKeyInQuery(aim_url, kRequestIdParameterKey,
                                          &vsrid_value));

  // Assert: Visual input type is NOT added to unimodal text queries.
  std::string vit_value;
  EXPECT_FALSE(net::GetValueForKeyInQuery(aim_url, kVisualInputTypeParameterKey,
                                          &vit_value));

  // Assert: Gsession id is NOT added to unimodal text queries.
  std::string gsession_id_value;
  EXPECT_FALSE(net::GetValueForKeyInQuery(aim_url, kSessionIdQueryParameterKey,
                                          &gsession_id_value));
}

TEST_F(ComposeboxQueryControllerTest, SuggestInputsForFirstDocument) {
  CreateController(
      /* send_lns_surface= */ false,
      /* suppress_lns_surface_param_if_no_image= */ true,
      /* enable_multi_context_input_flow= */ false,
      /* enable_viewport_images= */ true,
      /* use_separate_request_ids_for_multi_context_viewport_images= */ true,
      /* enable_cluster_info_ttl= */ false,
      /* prioritize_suggestions_for_the_first_attached_document= */ true);
  StartSession();

  auto pdf_token = UploadSimpleTestAttachment(lens::MimeType::kPdf);
  auto tab_token =
      UploadSimpleTestAttachment(lens::MimeType::kAnnotatedPageContent);
  auto image_token = UploadSimpleTestAttachment(lens::MimeType::kImage);

  {
    // Verify that when [1] pdf and [2] tab are attached, the [1] pdf is
    // used to serve suggestions.
    auto inputs = controller().CreateSuggestInputs({pdf_token, tab_token});

    EXPECT_EQ(inputs->encoded_request_id(),
              GetEncodedRequestInfoForToken(pdf_token));
    EXPECT_EQ(inputs->contextual_visual_input_type(), "pdf");
  }

  {
    // Verify that when [1] tab and [2] pdf are attached, the [1] tab is
    // used to serve suggestions.
    auto inputs = controller().CreateSuggestInputs({tab_token, pdf_token});

    EXPECT_EQ(inputs->encoded_request_id(),
              GetEncodedRequestInfoForToken(tab_token));
    EXPECT_EQ(inputs->contextual_visual_input_type(), "wp");
  }

  {
    // Verify that when [1] image and [2] pdf are attached, the [2] pdf is
    // used to serve suggestions.
    auto inputs = controller().CreateSuggestInputs({image_token, pdf_token});

    EXPECT_EQ(inputs->encoded_request_id(),
              GetEncodedRequestInfoForToken(pdf_token));
    EXPECT_EQ(inputs->contextual_visual_input_type(), "pdf");
  }

  {
    // Verify that when [1] image and [2] tab are attached, the [2] tab is
    // used to serve suggestions.
    auto inputs = controller().CreateSuggestInputs({image_token, tab_token});

    EXPECT_EQ(inputs->encoded_request_id(),
              GetEncodedRequestInfoForToken(tab_token));
    EXPECT_EQ(inputs->contextual_visual_input_type(), "wp");
  }

  {
    // Verify that when image is the sole attachment, it is used to serve
    // suggestions.
    auto inputs = controller().CreateSuggestInputs({image_token});

    EXPECT_EQ(inputs->encoded_request_id(),
              GetEncodedRequestInfoForToken(image_token));
    EXPECT_EQ(inputs->contextual_visual_input_type(), "img");
  }
}

TEST_F(ComposeboxQueryControllerTest, SuggestInputsForOnlyAttachment) {
  // Use the Controller with implicit parameters set to prioritize the only
  // attachment.
  StartSession();

  auto pdf_token = UploadSimpleTestAttachment(lens::MimeType::kPdf);
  auto tab_token =
      UploadSimpleTestAttachment(lens::MimeType::kAnnotatedPageContent);

  {
    // Check that the request id is set correctly with a single pdf attachment
    auto inputs = controller().CreateSuggestInputs({pdf_token});
    EXPECT_EQ(inputs->encoded_request_id(),
              GetEncodedRequestInfoForToken(pdf_token));
    EXPECT_EQ(inputs->contextual_visual_input_type(), "pdf");
  }

  {
    // Check that the request id is set correctly with a single tab attachment
    auto inputs = controller().CreateSuggestInputs({tab_token});
    EXPECT_EQ(inputs->encoded_request_id(),
              GetEncodedRequestInfoForToken(tab_token));
    EXPECT_EQ(inputs->contextual_visual_input_type(), "wp");
  }

  {
    // Check that multiple attachments result in no suggest inputs.
    auto inputs = controller().CreateSuggestInputs({pdf_token, tab_token});

    EXPECT_FALSE(inputs->has_encoded_request_id());
    EXPECT_FALSE(inputs->has_search_session_id());
  }
}

TEST_F(ComposeboxQueryControllerTest, DeleteFile_Failed) {
  identity_test_env()->MakePrimaryAccountAvailable(
      kTestUser, signin::ConsentLevel::kSignin);

  // Wait until the state changes to kClusterInfoReceived.
  base::RunLoop cluster_info_run_loop;
  controller().set_on_query_controller_state_changed_callback(
      base::BindLambdaForTesting([&](QueryControllerState state) {
        if (state == QueryControllerState::kClusterInfoReceived) {
          cluster_info_run_loop.Quit();
        }
      }));

  // Start the session.
  controller().InitializeIfNeeded();

  // Delete file.
  const bool deleted =
      controller().DeleteFile(base::UnguessableToken::Create());

  EXPECT_FALSE(deleted);
}

TEST_F(ComposeboxQueryControllerTest, ClearFiles) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Check that file is in cache.
  EXPECT_TRUE(controller().GetFileInfoForTesting(file_token));

  // Clear files.
  controller().ClearFiles();

  // Check that file is no longer in cache.
  EXPECT_FALSE(controller().GetFileInfoForTesting(file_token));
}

TEST_F(ComposeboxQueryControllerTest,
       QuerySubmittedWithLnsSurfaceAndNoImageSuppressed) {
  CreateController(/*send_lns_surface=*/true,
                   /*suppress_lns_surface_param_if_no_image=*/true);

  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Act: Create the destination URL for the query. The destination URL can
  // only be created after the cluster info is received.
  std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info =
      std::make_unique<CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = "hello";
  search_url_request_info->query_start_time = kTestQueryStartTime;
  search_url_request_info->file_tokens.push_back(file_token);
  base::test::TestFuture<GURL> url_future;
  controller().CreateSearchUrl(std::move(search_url_request_info),
                               url_future.GetCallback());
  GURL aim_url = url_future.Take();

  // Assert: Lns surface is empty since it was suppressed due to no image.
  std::string lns_surface_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(aim_url, kLnsSurfaceParameterKey,
                                         &lns_surface_value));
  EXPECT_EQ(lns_surface_value, "");
}

TEST_F(ComposeboxQueryControllerTest, QuerySubmittedWithLnsSurfaceAndNoImage) {
  CreateController(/*send_lns_surface=*/true,
                   /*suppress_lns_surface_param_if_no_image=*/false);

  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Act: Create the destination URL for the query. The destination URL can
  // only be created after the cluster info is received.
  std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info =
      std::make_unique<CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = "hello";
  search_url_request_info->query_start_time = kTestQueryStartTime;
  search_url_request_info->file_tokens.push_back(file_token);
  base::test::TestFuture<GURL> url_future;
  controller().CreateSearchUrl(std::move(search_url_request_info),
                               url_future.GetCallback());
  GURL aim_url = url_future.Take();

  // Assert: Lns surface is added to the url.
  std::string lns_surface_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(aim_url, kLnsSurfaceParameterKey,
                                         &lns_surface_value));
  EXPECT_EQ(lns_surface_value, "42");
}

TEST_F(ComposeboxQueryControllerTest,
       MultipleUploadedPdf_HasCorrectRequestIds) {
  CreateController(/*send_lns_surface=*/false,
                   /*suppress_lns_surface_param_if_no_image=*/true,
                   /*enable_multi_context_input_flow=*/true);

  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the first file upload flow.
  const base::UnguessableToken first_file_token =
      base::UnguessableToken::Create();
  StartPdfFileUploadFlow(first_file_token,
                         /*file_data=*/std::vector<uint8_t>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(first_file_token, lens::MimeType::kPdf);

  auto first_file_upload_request = controller().last_sent_file_upload_request();

  // Act: Start the second file upload flow.
  const base::UnguessableToken second_file_token =
      base::UnguessableToken::Create();
  StartPdfFileUploadFlow(second_file_token,
                         /*file_data=*/std::vector<uint8_t>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(second_file_token, lens::MimeType::kPdf);

  auto second_file_upload_request =
      controller().last_sent_file_upload_request();

  // Validate the file upload request payloads.
  EXPECT_EQ(first_file_upload_request->objects_request()
                .payload()
                .content()
                .content_data(0)
                .content_type(),
            lens::ContentData::CONTENT_TYPE_PDF);
  EXPECT_EQ(second_file_upload_request->objects_request()
                .payload()
                .content()
                .content_data(0)
                .content_type(),
            lens::ContentData::CONTENT_TYPE_PDF);
  // Check that the vsrid matches that for the multi context flow.
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(first_file_token)
                ->GetRequestIdForTesting()
                .sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(second_file_token)
                ->GetRequestIdForTesting()
                .sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(first_file_token)
                ->GetRequestIdForTesting()
                .image_sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(second_file_token)
                ->GetRequestIdForTesting()
                .image_sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(first_file_token)
                ->GetRequestIdForTesting()
                .long_context_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(second_file_token)
                ->GetRequestIdForTesting()
                .long_context_id(),
            1);
  EXPECT_EQ(first_file_upload_request->objects_request()
                .request_context()
                .request_id()
                .sequence_id(),
            1);
  EXPECT_EQ(second_file_upload_request->objects_request()
                .request_context()
                .request_id()
                .sequence_id(),
            1);
  EXPECT_EQ(first_file_upload_request->objects_request()
                .request_context()
                .request_id()
                .image_sequence_id(),
            1);
  EXPECT_EQ(second_file_upload_request->objects_request()
                .request_context()
                .request_id()
                .image_sequence_id(),
            1);
  EXPECT_EQ(first_file_upload_request->objects_request()
                .request_context()
                .request_id()
                .long_context_id(),
            1);
  EXPECT_EQ(second_file_upload_request->objects_request()
                .request_context()
                .request_id()
                .long_context_id(),
            1);
  EXPECT_EQ(first_file_upload_request->objects_request()
                .request_context()
                .request_id()
                .media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_PDF);
  EXPECT_EQ(second_file_upload_request->objects_request()
                .request_context()
                .request_id()
                .media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_PDF);
  EXPECT_NE(first_file_upload_request->objects_request()
                .request_context()
                .request_id()
                .uuid(),
            second_file_upload_request->objects_request()
                .request_context()
                .request_id()
                .uuid());
  // Act: Create the destination URL for the query.
  std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info =
      std::make_unique<CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = "hello";
  search_url_request_info->query_start_time = kTestQueryStartTime;
  search_url_request_info->file_tokens.push_back(first_file_token);
  search_url_request_info->file_tokens.push_back(second_file_token);
  base::test::TestFuture<GURL> url_future;
  controller().CreateSearchUrl(std::move(search_url_request_info),
                               url_future.GetCallback());
  GURL aim_url = url_future.Take();

  // Assert: Lens request id is NOT added to queries using multi-context flow.
  std::string vsrid_value;
  EXPECT_FALSE(net::GetValueForKeyInQuery(aim_url, kRequestIdParameterKey,
                                          &vsrid_value));

  // Assert: Visual input type is NOT set for queries using multi-context flow.
  std::string vit_value;
  EXPECT_FALSE(net::GetValueForKeyInQuery(aim_url, kVisualInputTypeParameterKey,
                                          &vit_value));

  // Assert: Gsession id is added to multimodal queries.
  std::string gsession_id_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(aim_url, kSessionIdQueryParameterKey,
                                         &gsession_id_value));
  EXPECT_EQ(kTestSearchSessionId, gsession_id_value);

  // Check that the timestamps are attached to the url.
  std::string qsubts_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      aim_url, kQuerySubmissionTimeQueryParameter, &qsubts_value));

  std::string cud_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      aim_url, kClientUploadDurationQueryParameter, &cud_value));

  // Check that the contextual inputs param contains the request ids.
  lens::LensOverlayContextualInputs contextual_inputs =
      GetContextualInputsFromUrl(aim_url.spec());
  EXPECT_EQ(contextual_inputs.inputs_size(), 2);

  // The files may be in any order, so find the corresponding request ids.
  bool first_contextual_input_is_first_file =
      contextual_inputs.inputs(0).request_id().uuid() ==
      controller()
          .GetFileInfoForTesting(first_file_token)
          ->GetRequestIdForTesting()
          .uuid();
  auto first_file_request_id =
      controller()
          .GetFileInfoForTesting(first_contextual_input_is_first_file
                                     ? first_file_token
                                     : second_file_token)
          ->GetRequestIdForTesting();
  auto second_file_request_id =
      controller()
          .GetFileInfoForTesting(first_contextual_input_is_first_file
                                     ? second_file_token
                                     : first_file_token)
          ->GetRequestIdForTesting();

  EXPECT_THAT(contextual_inputs.inputs(0).request_id(),
              EqualsProto(first_file_request_id));
  EXPECT_THAT(contextual_inputs.inputs(1).request_id(),
              EqualsProto(second_file_request_id));
}

TEST_F(ComposeboxQueryControllerTest, UploadFileResponseSetsResponseBodies) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Arrange: Create a fake response with text.
  lens::LensOverlayServerResponse file_upload_response;
  file_upload_response.mutable_objects_response()
      ->mutable_text()
      ->set_content_language("en");
  file_upload_response.mutable_objects_response()->add_overlay_objects();

  controller().set_fake_file_upload_response(file_upload_response);

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token, /*file_data=*/std::vector<uint8_t>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Assert: Verify viewport text is set.
  auto* file_info = controller().GetFileInfoForTesting(file_token);
  ASSERT_TRUE(file_info);
  EXPECT_EQ(file_info->response_bodies.size(), 1u);
  lens::LensOverlayServerResponse server_response;
  ASSERT_TRUE(server_response.ParseFromString(file_info->response_bodies[0]));
  EXPECT_TRUE(server_response.has_objects_response());
  EXPECT_EQ(server_response.objects_response().text().content_language(), "en");
  EXPECT_EQ(server_response.objects_response().overlay_objects().size(), 1);
}

TEST_F(ComposeboxQueryControllerTest,
       UploadFileBeforeClusterInfoUpdatesRequestId) {
  // Act: Start the file upload flow BEFORE cluster info is received.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>());

  // Act: Initialize the session (fetches cluster info).
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Assert: File upload should proceed and succeed.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Assert: The request ID in the file info should now have the routing info.
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .routing_info()
                .cell_address(),
            kTestCellAddress);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .routing_info()
                .server_address(),
            kTestServerAddress);

  // Assert: The actual sent request should also have the routing info.
  auto last_request = controller().last_sent_file_upload_request();
  ASSERT_TRUE(last_request.has_value());
  EXPECT_EQ(last_request->objects_request()
                .request_context()
                .request_id()
                .routing_info()
                .cell_address(),
            kTestCellAddress);
  EXPECT_EQ(last_request->objects_request()
                .request_context()
                .request_id()
                .routing_info()
                .server_address(),
            kTestServerAddress);
}

TEST_F(ComposeboxQueryControllerTest, CreateSuggestInputsWithPageTitleAndUrl) {
  // Arrange: Create controller with
  // attach_page_title_and_url_to_suggest_requests enabled.
  CreateController(
      /*send_lns_surface=*/false,
      /*suppress_lns_surface_param_if_no_image=*/true,
      /*enable_multi_context_input_flow=*/false,
      /*enable_viewport_images=*/true,
      /*use_separate_request_ids_for_multi_context_viewport_images=*/true,
      /*enable_cluster_info_ttl=*/false,
      /*prioritize_suggestions_for_the_first_attached_document=*/false,
      /*attach_page_title_and_url_to_suggest_requests=*/true);
  StartSession();

  // Act: Start the file upload flow with page title and url.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  auto input_data = std::make_unique<lens::ContextualInputData>();
  input_data->primary_content_type = lens::MimeType::kAnnotatedPageContent;
  input_data->context_input = std::vector<lens::ContextualInput>();
  input_data->page_url = GURL("https://page.url");
  input_data->page_title = "Page Title";
  input_data->context_input->emplace_back(lens::ContextualInput(
      std::vector<uint8_t>(), lens::MimeType::kAnnotatedPageContent));
  input_data->is_page_context_eligible = true;
  controller().StartFileUploadFlow(file_token, std::move(input_data),
                                   std::nullopt);
  WaitForFileUpload(file_token, lens::MimeType::kAnnotatedPageContent);

  // Act: Create suggest inputs.
  auto suggest_inputs = controller().CreateSuggestInputs({file_token});

  // Assert: Verify page title and url are attached.
  EXPECT_TRUE(suggest_inputs->send_page_title_and_url());
  EXPECT_EQ(suggest_inputs->page_title(), "Page Title");
  EXPECT_EQ(suggest_inputs->page_url(), "https://page.url/");
}

TEST_F(ComposeboxQueryControllerTest, QuerySubmittedWithInvocationSource) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Act: Create the destination URL for the query.
  std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info =
      std::make_unique<CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = "hello";
  search_url_request_info->query_start_time = kTestQueryStartTime;
  search_url_request_info->file_tokens.push_back(file_token);
  search_url_request_info->invocation_source =
      lens::LensOverlayInvocationSource::kAppMenu;

  base::test::TestFuture<GURL> url_future;
  controller().CreateSearchUrl(std::move(search_url_request_info),
                               url_future.GetCallback());
  GURL search_url = url_future.Take();

  // Assert: Invocation source is added to the url.
  std::string source_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(search_url, "source", &source_value));
  EXPECT_EQ(source_value, "chrome.crn.menu");
}

TEST_F(ComposeboxQueryControllerTest, ContextualTasksOverrides) {
  // Arrange: Enable ContextualTasks.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      contextual_tasks::kContextualTasks,
      {{"ForceContextIdMigration", "true"}});

  // Create controller with flags disabled initially.
  CreateController(
      /*send_lns_surface=*/false,
      /*suppress_lns_surface_param_if_no_image=*/true,
      /*enable_multi_context_input_flow=*/false,
      /*enable_viewport_images=*/true,
      /*use_separate_request_ids_for_multi_context_viewport_images=*/false,
      /*enable_cluster_info_ttl=*/false,
      /*prioritize_suggestions_for_the_first_attached_document=*/false);

  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  int64_t context_id = 12345;
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>(), context_id);

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Check that the vsrid matches that for a pdf upload using the context_id
  // migration flow
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .image_sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .long_context_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfoForTesting(file_token)
                ->GetRequestIdForTesting()
                .context_id(),
            context_id);
}

TEST_F(ComposeboxQueryControllerTest, HandleInteractionResponse) {
  // Act: Start the session.
  controller().InitializeIfNeeded();
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(file_token,
                         /*file_data=*/std::vector<uint8_t>());
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Arrange: Set up the fake interaction response.
  lens::LensOverlayServerResponse interaction_response;
  interaction_response.mutable_interaction_response()
      ->mutable_text()
      ->set_content_language("en");
  controller().set_fake_interaction_response(interaction_response);

  // Act: Create the destination URL for the query.
  std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info =
      std::make_unique<CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = "hello";
  search_url_request_info->search_url_type =
      ComposeboxQueryController::SearchUrlType::kStandard;
  search_url_request_info->query_start_time = kTestQueryStartTime;
  search_url_request_info->lens_overlay_selection_type =
      lens::LensOverlaySelectionType::REGION_SEARCH;
  search_url_request_info->image_crop = lens::ImageCrop();
  search_url_request_info->image_crop->mutable_zoomed_crop()
      ->mutable_crop()
      ->set_coordinate_type(lens::CoordinateType::NORMALIZED);
  search_url_request_info->image_crop->mutable_zoomed_crop()->set_zoom(1);
  search_url_request_info->image_crop->mutable_zoomed_crop()->set_parent_height(
      25);
  search_url_request_info->file_tokens.push_back(file_token);
  search_url_request_info->client_logs = lens::LensOverlayClientLogs();

  // Arrange: Set up the callback to verify the response.
  base::test::TestFuture<lens::LensOverlayInteractionResponse> response_future;
  search_url_request_info->interaction_response_callback =
      response_future.GetCallback();

  // Act: Send the request.
  base::test::TestFuture<GURL> url_future;
  controller().CreateSearchUrl(std::move(search_url_request_info),
                               url_future.GetCallback());
  GURL search_url = url_future.Take();

  // Assert: Verify the interaction response is passed to the callback.
  lens::LensOverlayInteractionResponse actual_response = response_future.Take();
  EXPECT_EQ(actual_response.text().content_language(), "en");
}

#if !BUILDFLAG(IS_IOS)
TEST_F(ComposeboxQueryControllerTest,
       CreateClientToAimRequestIncludesVisualSearchInteractionData) {
  // Act: Start the session.
  controller().InitializeIfNeeded();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  std::vector<uint8_t> image_bytes = CreateJPGBytes(100, 100);
  lens::ImageEncodingOptions image_options{.max_size = 1000000,
                                           .max_height = 1000,
                                           .max_width = 1000,
                                           .compression_quality = 30};
  StartImageFileUploadFlow(file_token, image_bytes, image_options);

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kImage);

  // Create the client to aim request info.
  auto create_client_to_aim_request_info =
      std::make_unique<CreateClientToAimRequestInfo>();
  create_client_to_aim_request_info->query_text = "test query";
  create_client_to_aim_request_info->file_tokens = {file_token};

  // Act: Create the client to aim request.
  auto client_to_aim_message = controller().CreateClientToAimRequest(
      std::move(create_client_to_aim_request_info));

  // Assert: Verify that the visual search interaction data is included.
  ASSERT_EQ(client_to_aim_message.submit_query()
                .payload()
                .lens_image_query_data_size(),
            1);
  const auto& lens_image_query_data =
      client_to_aim_message.submit_query().payload().lens_image_query_data(0);
  EXPECT_TRUE(lens_image_query_data.has_visual_search_interaction_data());
  const auto& interaction_data =
      lens_image_query_data.visual_search_interaction_data();
  EXPECT_EQ(interaction_data.log_data().user_selection_data().selection_type(),
            lens::MULTIMODAL_SEARCH);
  EXPECT_EQ(interaction_data.interaction_type(),
            lens::LensOverlayInteractionRequestMetadata::REGION);
  // Verify default full region crop.
  EXPECT_TRUE(interaction_data.has_zoomed_crop());
  EXPECT_FLOAT_EQ(interaction_data.zoomed_crop().crop().center_x(), 0.5f);
  EXPECT_FLOAT_EQ(interaction_data.zoomed_crop().crop().center_y(), 0.5f);
  EXPECT_FLOAT_EQ(interaction_data.zoomed_crop().crop().width(), 1.0f);
  EXPECT_FLOAT_EQ(interaction_data.zoomed_crop().crop().height(), 1.0f);
}
#endif  // !BUILDFLAG(IS_IOS)

}  // namespace contextual_search
