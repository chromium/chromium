// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/composebox/composebox_query_controller.h"

#include <memory>
#include <optional>
#include <string>

#include "base/base64url.h"
#include "base/test/bind.h"
#include "base/test/repeating_test_future.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/version_info/channel.h"
#include "components/omnibox/composebox/composebox_query.mojom.h"
#include "components/omnibox/composebox/test_composebox_query_controller.h"
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
#include "third_party/lens_server_proto/lens_overlay_platform.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "third_party/lens_server_proto/lens_overlay_surface.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia_operations.h"

#if !BUILDFLAG(IS_IOS)
#include "ui/gfx/codec/jpeg_codec.h"
#endif  // !BUILDFLAG(IS_IOS)

constexpr char kQuerySubmissionTimeQueryParameter[] = "qsubts";
constexpr char kClientUploadDurationQueryParameter[] = "cud";
constexpr char kSessionIdQueryParameterKey[] = "gsessionid";
constexpr char kVariationsHeaderKey[] = "X-Client-Data";
constexpr char kTestUser[] = "test_user@gmail.com";
constexpr char kTestSearchSessionId[] = "test_search_session_id";
constexpr char kTestServerSessionId[] = "test_server_session_id";
constexpr char kLocale[] = "en-US";
constexpr char kRegion[] = "US";
constexpr char kTimeZone[] = "America/Los_Angeles";
constexpr char kRequestIdParameterKey[] = "vsrid";
constexpr char kVisualInputTypeParameterKey[] = "vit";
constexpr char kLnsSurfaceParameterKey[] = "lns_surface";
constexpr char kTestCellAddress[] = "test_cell_address";
constexpr char kTestServerAddress[] = "test_server_address";
base::Time kTestQueryStartTime =
    base::Time::FromMillisecondsSinceUnixEpoch(1000);

using FileUploadStatusTuple = std::tuple<base::UnguessableToken,
                                         lens::MimeType,
                                         FileUploadStatus,
                                         std::optional<FileUploadErrorType>>;

class ComposeboxQueryControllerTest
    : public testing::Test,
      public ComposeboxQueryController::FileUploadStatusObserver {
 public:
  ComposeboxQueryControllerTest() = default;
  ~ComposeboxQueryControllerTest() override = default;

  void CreateController(bool send_lns_surface) {
    controller_ = std::make_unique<TestComposeboxQueryController>(
        identity_manager(), shared_url_loader_factory_,
        version_info::Channel::UNKNOWN, kLocale, template_url_service(),
        fake_variations_client_.get(), send_lns_surface);
    controller_->AddObserver(this);

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
                              QueryControllerState::kClusterInfoReceived) {
    EXPECT_EQ(QueryControllerState::kAwaitingClusterInfoResponse,
              controller_state_future_.Take());
    EXPECT_EQ(QueryControllerState::kAwaitingClusterInfoResponse,
              controller().query_controller_state());

    EXPECT_EQ(expected_state, controller_state_future_.Take());
    EXPECT_EQ(expected_state, controller().query_controller_state());

    EXPECT_EQ(controller().num_cluster_info_fetch_requests_sent(), 1);
    // The cluster info request should have the cors variations header.
    EXPECT_THAT(controller().last_sent_cors_exempt_headers(),
                testing::Contains(kVariationsHeaderKey));
  }

  void StartPdfFileUploadFlow(const base::UnguessableToken& file_token,
                              scoped_refptr<base::RefCountedBytes> file_data) {
    std::unique_ptr<ComposeboxQueryController::FileInfo> file_info =
        std::make_unique<ComposeboxQueryController::FileInfo>();
    file_info->file_token_ = file_token;
    file_info->mime_type_ = lens::MimeType::kPdf;
    controller().StartFileUploadFlow(std::move(file_info), std::move(file_data),
                                     /*image_options=*/std::nullopt);
  }

  void StartImageFileUploadFlow(const base::UnguessableToken& file_token,
                                scoped_refptr<base::RefCountedBytes> file_data,
                                std::optional<composebox::ImageEncodingOptions>
                                    image_options = std::nullopt) {
    std::unique_ptr<ComposeboxQueryController::FileInfo> file_info =
        std::make_unique<ComposeboxQueryController::FileInfo>();
    file_info->file_token_ = file_token;
    file_info->mime_type_ = lens::MimeType::kImage;
    controller().StartFileUploadFlow(std::move(file_info), std::move(file_data),
                                     image_options);
  }

  void WaitForFileUpload(
      const base::UnguessableToken& file_token,
      lens::MimeType mime_type,
      FileUploadStatus expected_status = FileUploadStatus::kUploadSuccessful,
      std::optional<FileUploadErrorType> expected_error_type = std::nullopt) {
    FileUploadStatusTuple processing_file_upload_status =
        file_upload_status_future_.Take();
    EXPECT_EQ(file_token, std::get<0>(processing_file_upload_status));
    EXPECT_EQ(mime_type, std::get<1>(processing_file_upload_status));
    EXPECT_EQ(FileUploadStatus::kProcessing,
              std::get<2>(processing_file_upload_status));
    EXPECT_EQ(std::nullopt, std::get<3>(processing_file_upload_status));

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
      EXPECT_EQ(controller().num_file_upload_requests_sent(), 1);
      EXPECT_THAT(GetGsessionIdFromUrl(controller().last_sent_fetch_url()),
                  testing::Optional(std::string(kTestServerSessionId)));
      // The file upload request should have the cors variations header.
      EXPECT_THAT(controller().last_sent_cors_exempt_headers(),
                  testing::Contains(kVariationsHeaderKey));
    }
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
       NotifySessionStartedIssuesClusterInfoRequest) {
  // Act: Start the session.
  controller().NotifySessionStarted();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();
}

TEST_F(ComposeboxQueryControllerTest,
       NotifySessionStartedIssuesClusterInfoRequestWithOAuth) {
  // Arrange: Make primary account available.
  identity_test_env()->MakePrimaryAccountAvailable(
      kTestUser, signin::ConsentLevel::kSignin);

  // Act: Start the session.
  controller().NotifySessionStarted();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);

  // Assert: Validate cluster info request and state changes
  WaitForClusterInfo();
}

TEST_F(ComposeboxQueryControllerTest,
       NotifySessionStartedIssuesClusterInfoRequestFailure) {
  // Arrange: Simulate an error in the cluster info request.
  controller().set_next_cluster_info_request_should_return_error(true);

  // Act: Start the session.
  controller().NotifySessionStarted();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo(
      /*expected_state=*/QueryControllerState::kClusterInfoInvalid);
}

TEST_F(ComposeboxQueryControllerTest, NotifySessionAbandoned) {
  // Act: Start the session.
  controller().NotifySessionStarted();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(
      file_token,
      /*file_data=*/base::MakeRefCounted<base::RefCountedBytes>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Check that file is in cache.
  EXPECT_TRUE(controller().GetFileInfo(file_token));

  // Act: End the session.
  controller().NotifySessionAbandoned();

  // Check that file is no longer in cache.
  EXPECT_FALSE(controller().GetFileInfo(file_token));
  EXPECT_EQ(QueryControllerState::kOff, controller().query_controller_state());
}

TEST_F(ComposeboxQueryControllerTest, UploadFileRequestFailure) {
  // Act: Start the session.
  controller().NotifySessionStarted();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Arrange: Simulate a failure in the file upload request.
  controller().set_next_file_upload_request_should_return_error(true);

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(
      file_token,
      /*file_data=*/base::MakeRefCounted<base::RefCountedBytes>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf,
                    /*expected_status=*/FileUploadStatus::kUploadFailed,
                    /*expected_error_type=*/FileUploadErrorType::kServerError);
}

#if !BUILDFLAG(IS_IOS)
TEST_F(ComposeboxQueryControllerTest, UploadImageFileRequestSuccess) {
  // Act: Start the session.
  controller().NotifySessionStarted();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  std::vector<uint8_t> image_bytes = CreateJPGBytes(100, 100);
  composebox::ImageEncodingOptions image_options{.max_size = 1000000,
                                                 .max_height = 1000,
                                                 .max_width = 1000,
                                                 .compression_quality = 30};
  StartImageFileUploadFlow(
      file_token,
      /*file_data=*/base::MakeRefCounted<base::RefCountedBytes>(image_bytes),
      image_options);

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
                .GetFileInfo(file_token)
                ->GetRequestIdForTesting()
                ->sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfo(file_token)
                ->GetRequestIdForTesting()
                ->image_sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfo(file_token)
                ->GetRequestIdForTesting()
                ->long_context_id(),
            0);
  // Check that the routing info is in the vsrid.
  EXPECT_EQ(controller()
                .GetFileInfo(file_token)
                ->GetRequestIdForTesting()
                ->routing_info()
                .cell_address(),
            kTestCellAddress);
  EXPECT_EQ(controller()
                .GetFileInfo(file_token)
                ->GetRequestIdForTesting()
                ->routing_info()
                .server_address(),
            kTestServerAddress);
}

TEST_F(ComposeboxQueryControllerTest, UploadEmptyImageFileRequestFailure) {
  // Act: Start the session.
  controller().NotifySessionStarted();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  std::vector<uint8_t> image_bytes = std::vector<uint8_t>();
  composebox::ImageEncodingOptions image_options{.max_size = 1000000,
                                                 .max_height = 1000,
                                                 .max_width = 1000,
                                                 .compression_quality = 30};
  StartImageFileUploadFlow(
      file_token,
      /*file_data=*/base::MakeRefCounted<base::RefCountedBytes>(image_bytes),
      image_options);

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kImage,
                    FileUploadStatus::kValidationFailed,
                    FileUploadErrorType::kImageProcessingError);
}
#endif  // !BUILDFLAG(IS_IOS)

TEST_F(ComposeboxQueryControllerTest, UploadPdfFileRequestSuccess) {
  // Act: Start the session.
  controller().NotifySessionStarted();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(
      file_token,
      /*file_data=*/base::MakeRefCounted<base::RefCountedBytes>());

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
                .data(),
            "");
  // Check that the vsrid matches that for a pdf upload.
  EXPECT_EQ(controller()
                .GetFileInfo(file_token)
                ->GetRequestIdForTesting()
                ->sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfo(file_token)
                ->GetRequestIdForTesting()
                ->image_sequence_id(),
            0);
  EXPECT_EQ(controller()
                .GetFileInfo(file_token)
                ->GetRequestIdForTesting()
                ->long_context_id(),
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
                .GetFileInfo(file_token)
                ->GetRequestIdForTesting()
                ->routing_info()
                .cell_address(),
            kTestCellAddress);
  EXPECT_EQ(controller()
                .GetFileInfo(file_token)
                ->GetRequestIdForTesting()
                ->routing_info()
                .server_address(),
            kTestServerAddress);
}

TEST_F(ComposeboxQueryControllerTest, UploadInvalidMimeTypeFileRequestFailure) {
  // Act: Start the session.
  controller().NotifySessionStarted();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();

  std::unique_ptr<ComposeboxQueryController::FileInfo> file_info =
      std::make_unique<ComposeboxQueryController::FileInfo>();
  file_info->file_token_ = file_token;
  lens::MimeType mime_type = lens::MimeType::kUnknown;
  file_info->mime_type_ = mime_type;
  controller().StartFileUploadFlow(
      std::move(file_info), base::MakeRefCounted<base::RefCountedBytes>(),
      /*image_options=*/std::nullopt);

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, mime_type, FileUploadStatus::kValidationFailed,
                    FileUploadErrorType::kBrowserProcessingError);
}

TEST_F(ComposeboxQueryControllerTest, UploadFileRequestSuccessWithOAuth) {
  // Arrange: Make primary account available.
  identity_test_env()->MakePrimaryAccountAvailable(
      kTestUser, signin::ConsentLevel::kSignin);

  // Act: Start the session.
  controller().NotifySessionStarted();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(
      file_token,
      /*file_data=*/base::MakeRefCounted<base::RefCountedBytes>());
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
  controller().NotifySessionStarted();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(
      file_token,
      /*file_data=*/base::MakeRefCounted<base::RefCountedBytes>());

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
  controller().NotifySessionStarted();

  // Act: Start the file upload flow without waiting for the cluster info
  // request to complete.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(
      file_token,
      /*file_data=*/base::MakeRefCounted<base::RefCountedBytes>());

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
  EXPECT_EQ(client_context.surface(), lens::SURFACE_CHROME_NTP);
  EXPECT_EQ(client_context.platform(), lens::PLATFORM_LENS_OVERLAY);
  EXPECT_EQ(client_context.locale_context().language(), kLocale);
  EXPECT_EQ(client_context.locale_context().region(), kRegion);
  EXPECT_EQ(client_context.locale_context().time_zone(), kTimeZone);
}

TEST_F(ComposeboxQueryControllerTest, AbandonSessionClearsFiles) {
  // Act: Start the session.
  controller().NotifySessionStarted();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(
      file_token,
      /*file_data=*/base::MakeRefCounted<base::RefCountedBytes>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Act: Abandon the session.
  controller().NotifySessionAbandoned();

  // Assert: Validate the state change.
  EXPECT_EQ(QueryControllerState::kOff, controller_state_future_.Take());

  // Act: Start the session again.
  controller().NotifySessionStarted();

  // Assert: Validate the state change.
  EXPECT_EQ(QueryControllerState::kAwaitingClusterInfoResponse,
            controller_state_future_.Take());

  // Assert: Validate the state change.
  EXPECT_EQ(QueryControllerState::kClusterInfoReceived,
            controller_state_future_.Take());

  // Act: Generate the destination URL for the query.
  GURL aim_url = controller().CreateAimUrl("test", kTestQueryStartTime);

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

TEST_F(ComposeboxQueryControllerTest,
       AbandonSessionPreventsMultipleClusterInfoFetch) {
  // Enable cluster info TTL.
  controller().set_enable_cluster_info_ttl(true);

  // Act: Start the session.
  controller().NotifySessionStarted();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Abandon the session.
  controller().NotifySessionAbandoned();

  // Assert: Validate the state change.
  EXPECT_EQ(QueryControllerState::kOff, controller_state_future_.Take());

  // Act: Start the session again.
  controller().NotifySessionStarted();

  // Assert: Validate the state change.
  EXPECT_EQ(QueryControllerState::kAwaitingClusterInfoResponse,
            controller_state_future_.Take());

  // Assert: Validate the state change.
  EXPECT_EQ(QueryControllerState::kClusterInfoReceived,
            controller_state_future_.Take());

  // Wait 45 minutes, long enough for the cluster info to expire once.
  task_environment().FastForwardBy(base::Minutes(45));

  // Assert: Validate the state change sequence.
  EXPECT_EQ(QueryControllerState::kClusterInfoInvalid,
            controller_state_future_.Take());
  EXPECT_EQ(QueryControllerState::kAwaitingClusterInfoResponse,
            controller_state_future_.Take());
  EXPECT_EQ(QueryControllerState::kClusterInfoReceived,
            controller_state_future_.Take());

  // Assert: The cluster info fetch request was only sent 3 times.
  EXPECT_EQ(controller().num_cluster_info_fetch_requests_sent(), 3);
}

TEST_F(ComposeboxQueryControllerTest,
       UnimodalTextQuerySubmittedWithInvalidClusterInfoSuccess) {
  controller().set_next_cluster_info_request_should_return_error(true);

  // Act: Start the session.
  controller().NotifySessionStarted();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo(QueryControllerState::kClusterInfoInvalid);

  // Act: Generate the destination URL for the query.
  GURL aim_url = controller().CreateAimUrl("test", kTestQueryStartTime);

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
  controller().NotifySessionStarted();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Generate the destination URL for the query.
  GURL aim_url = controller().CreateAimUrl("test", kTestQueryStartTime);

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

TEST_F(ComposeboxQueryControllerTest, QuerySubmittedWithUploadedPdf) {
  // Act: Start the session.
  controller().NotifySessionStarted();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(
      file_token,
      /*file_data=*/base::MakeRefCounted<base::RefCountedBytes>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Act: Create the destination URL for the query. The destination URL can
  // only be created after the cluster info is received.
  GURL aim_url = controller().CreateAimUrl("hello", kTestQueryStartTime);

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
}

#if !BUILDFLAG(IS_IOS)
TEST_F(ComposeboxQueryControllerTest, QuerySubmittedWithUploadedImage) {
  // Act: Start the session.
  controller().NotifySessionStarted();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  std::vector<uint8_t> image_bytes = CreateJPGBytes(100, 100);
  composebox::ImageEncodingOptions image_options{.max_size = 1000000,
                                                 .max_height = 1000,
                                                 .max_width = 1000,
                                                 .compression_quality = 30};
  StartImageFileUploadFlow(
      file_token,
      /*file_data=*/base::MakeRefCounted<base::RefCountedBytes>(image_bytes),
      image_options);

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kImage);

  // Act: Create the destination URL for the query. The destination URL can
  // only be created after the cluster info is received.
  GURL aim_url = controller().CreateAimUrl("hello", kTestQueryStartTime);

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
      aim_url, kClientUploadDurationQueryParameter,
      &cud_value));
}
#endif  // !BUILDFLAG(IS_IOS)

TEST_F(ComposeboxQueryControllerTest,
       QuerySubmittedWithUploadedPdfButInvalidClusterInfoIsUnimodal) {
  // Enable cluster info TTL.
  controller().set_enable_cluster_info_ttl(true);

  // Act: Start the session.
  controller().NotifySessionStarted();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Ensure that future cluster info requests fail.
  controller().set_next_cluster_info_request_should_return_error(true);

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(
      file_token,
      /*file_data=*/base::MakeRefCounted<base::RefCountedBytes>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Wait 1 hour.
  task_environment().FastForwardBy(base::Hours(1));

  // Assert: Validate cluster info request and state changes.
  EXPECT_EQ(QueryControllerState::kClusterInfoInvalid,
            controller().query_controller_state());

  // Act: Create the destination URL for the query.
  GURL aim_url = controller().CreateAimUrl("hello", kTestQueryStartTime);

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

TEST_F(ComposeboxQueryControllerTest, DeleteFile_Success) {
  // Act: Start the session.
  controller().NotifySessionStarted();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(
      file_token,
      /*file_data=*/base::MakeRefCounted<base::RefCountedBytes>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Check that file is in cache.
  EXPECT_TRUE(controller().GetFileInfo(file_token));

  // Delete file.
  const bool deleted = controller().DeleteFile(file_token);

  // Check that file is no longer in cache.
  EXPECT_TRUE(deleted);
  EXPECT_FALSE(controller().GetFileInfo(file_token));
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
  controller().NotifySessionStarted();

  // Delete file.
  const bool deleted =
      controller().DeleteFile(base::UnguessableToken::Create());

  EXPECT_FALSE(deleted);
}

TEST_F(ComposeboxQueryControllerTest, ClearFiles) {
  // Act: Start the session.
  controller().NotifySessionStarted();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(
      file_token,
      /*file_data=*/base::MakeRefCounted<base::RefCountedBytes>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Check that file is in cache.
  EXPECT_TRUE(controller().GetFileInfo(file_token));

  // Clear files.
  controller().ClearFiles();

  // Check that file is no longer in cache.
  EXPECT_FALSE(controller().GetFileInfo(file_token));
}

TEST_F(ComposeboxQueryControllerTest, QuerySubmittedWithLnsSurface) {
  CreateController(/*send_lns_surface=*/true);

  // Act: Start the session.
  controller().NotifySessionStarted();

  // Assert: Validate cluster info request and state changes.
  WaitForClusterInfo();

  // Act: Start the file upload flow.
  const base::UnguessableToken file_token = base::UnguessableToken::Create();
  StartPdfFileUploadFlow(
      file_token,
      /*file_data=*/base::MakeRefCounted<base::RefCountedBytes>());

  // Assert: Validate file upload request and status changes.
  WaitForFileUpload(file_token, lens::MimeType::kPdf);

  // Act: Create the destination URL for the query. The destination URL can
  // only be created after the cluster info is received.
  GURL aim_url = controller().CreateAimUrl("hello", kTestQueryStartTime);

  // Assert: Lns surface is added to the url.
  std::string lns_surface_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(aim_url, kLnsSurfaceParameterKey,
                                         &lns_surface_value));
  EXPECT_EQ(lns_surface_value, "47");
}
