// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"

#include <list>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_encoding.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager_test_api.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/signatures.h"
#include "components/prefs/pref_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_ids_provider.h"
#include "google_apis/common/api_key_request_test_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/third_party/mozilla/url_parse.h"

namespace autofill {
namespace {

using ::base::UTF8ToUTF16;
using mojom::SubmissionSource;
using ::net::test_server::BasicHttpResponse;
using ::net::test_server::EmbeddedTestServer;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;
using test::CreateTestFormField;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::SizeIs;

constexpr int METHOD_GET = 0;
constexpr int METHOD_POST = 1;
constexpr int CACHE_MISS = 0;
constexpr int CACHE_HIT = 1;

std::vector<raw_ptr<FormStructure, VectorExperimental>> ToRawPointerVector(
    const std::vector<std::unique_ptr<FormStructure>>& list) {
  std::vector<raw_ptr<FormStructure, VectorExperimental>> result;
  for (const auto& item : list) {
    result.push_back(item.get());
  }
  return result;
}

// Sets the `host_form_signature` member of all fields contained in
// `form_structure` to the signature of the `form_structure`.
void SetCorrectFieldHostFormSignatures(FormStructure& form_structure) {
  for (const std::unique_ptr<AutofillField>& field : form_structure) {
    field->set_host_form_signature(form_structure.form_signature());
  }
}

// Puts all data elements within the response body together in a single
// DataElement and return the buffered content as a string. This ensure all
// the response body data is utilized.
std::string GetStringFromDataElements(
    const std::vector<network::DataElement>* data_elements) {
  std::string result;
  for (const network::DataElement& e : *data_elements) {
    DCHECK_EQ(e.type(), network::DataElement::Tag::kBytes);
    // Provide the length of the bytes explicitly, not to rely on the null
    // termination.
    const auto piece = e.As<network::DataElementBytes>().AsStringPiece();
    result.append(piece);
  }
  return result;
}

// Gets the AutofillUploadRequest proto from the HTTP loader request payload.
// Will return false if failed to get the proto.
bool GetUploadRequestProtoFromRequest(
    network::TestURLLoaderFactory::PendingRequest* loader_request,
    AutofillUploadRequest* upload_request) {
  if (loader_request == nullptr) {
    return false;
  }

  if (loader_request->request.request_body == nullptr) {
    return false;
  }

  std::string request_body_content = GetStringFromDataElements(
      loader_request->request.request_body->elements());
  if (!upload_request->ParseFromString(request_body_content)) {
    return false;
  }
  return true;
}

bool GetAutofillPageResourceQueryRequestFromRequest(
    network::TestURLLoaderFactory::PendingRequest* loader_request,
    AutofillPageResourceQueryRequest* query_request) {
  if (loader_request == nullptr) {
    return false;
  }

  if (loader_request->request.request_body == nullptr) {
    return false;
  }

  std::string request_body_content = GetStringFromDataElements(
      loader_request->request.request_body->elements());
  if (!query_request->ParseFromString(request_body_content)) {
    return false;
  }
  return true;
}

bool DeserializeAutofillPageQueryRequest(std::string_view serialized_content,
                                         AutofillPageQueryRequest* request) {
  std::string decoded_content;
  if (!base::Base64UrlDecode(serialized_content,
                             base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                             &decoded_content)) {
    return false;
  }
  if (!request->ParseFromString(decoded_content)) {
    return false;
  }
  return true;
}

class AutofillCrowdsourcingManagerWithCustomPayloadSize
    : public AutofillCrowdsourcingManager {
 public:
  AutofillCrowdsourcingManagerWithCustomPayloadSize(AutofillClient* client,
                                                    const std::string& api_key,
                                                    size_t length)
      : AutofillCrowdsourcingManager(client,
                                     api_key,
                                     /*log_manager=*/nullptr),
        length_(length) {}
  ~AutofillCrowdsourcingManagerWithCustomPayloadSize() override = default;

 protected:
  size_t GetPayloadLength(std::string_view payload) const override {
    return length_;
  }

 private:
  size_t length_;
};

// This tests AutofillCrowdsourcingManager. AutofillCrowdsourcingManagerTest
// implements AutofillCrowdsourcingManager::Observer and creates an instance of
// AutofillCrowdsourcingManager. Then it records responses to different
// initiated requests, which are verified later. To mock network requests
// TestURLLoaderFactory is used, which creates SimpleURLLoaders that do not
// go over the wire, but allow calling back HTTP responses directly.
// The responses in test are out of order and verify: successful query request,
// successful upload request, failed upload request.
class AutofillCrowdsourcingManagerTest : public ::testing::Test {
 public:
  enum class ResponseType {
    kQuerySuccessful,
    kUploadSuccessful,
  };

  struct ResponseData {
    ResponseType type_of_response = ResponseType::kQuerySuccessful;
    std::string signature;
    std::string response;
  };

  AutofillCrowdsourcingManagerTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        crowdsourcing_manager_(
            AutofillCrowdsourcingManagerTestApi::CreateManagerForApiKey(
                &client_,
                /*api_key=*/"")) {
    client().set_shared_url_loader_factory(test_shared_loader_factory_);
  }

  base::WeakPtr<AutofillCrowdsourcingManagerTest> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  bool StartQueryRequest(
      const std::vector<std::unique_ptr<FormStructure>>& form_structures) {
    return crowdsourcing_manager().StartQueryRequest(
        ToRawPointerVector(form_structures), driver().GetIsolationInfo(),
        base::BindOnce(
            &AutofillCrowdsourcingManagerTest::OnLoadedServerPredictions,
            weak_ptr_factory_.GetWeakPtr()));
  }

  void OnLoadedServerPredictions(
      std::optional<AutofillCrowdsourcingManager::QueryResponse> response) {
    if (!response) {
      return;
    }
    responses().push_back({.type_of_response = ResponseType::kQuerySuccessful,
                           .signature = {},
                           .response = std::move(response->response)});
  }

  TestAutofillClient& client() { return client_; }
  TestAutofillDriver& driver() { return driver_; }
  AutofillCrowdsourcingManager& crowdsourcing_manager() {
    return *crowdsourcing_manager_;
  }
  std::list<ResponseData>& responses() { return responses_; }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }
  network::TestURLLoaderFactory& url_loader_factory() {
    return test_url_loader_factory_;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_environment_;
  ScopedActiveAutofillExperiments scoped_active_autofill_experiments;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  TestAutofillClient client_;
  TestAutofillDriver driver_{&client_};

  std::unique_ptr<AutofillCrowdsourcingManager> crowdsourcing_manager_;
  std::list<ResponseData> responses_;

  base::WeakPtrFactory<AutofillCrowdsourcingManagerTest> weak_ptr_factory_{
      this};
};

TEST_F(AutofillCrowdsourcingManagerTest, QueryAndUploadTest) {
  std::vector<std::unique_ptr<FormStructure>> form_structures;
  form_structures.push_back(std::make_unique<FormStructure>(
      test::GetFormData({.fields = {{.role = USERNAME},
                                    {.role = NAME_FIRST},
                                    {.role = NAME_LAST},
                                    {.role = EMAIL_ADDRESS},
                                    {.role = EMAIL_ADDRESS},
                                    {.role = PASSWORD}}})));
  form_structures.push_back(std::make_unique<FormStructure>(
      test::GetFormData({.fields = {{.role = ADDRESS_HOME_LINE1},
                                    {.role = ADDRESS_HOME_LINE2},
                                    {.role = ADDRESS_HOME_CITY}}})));
  form_structures.push_back(std::make_unique<FormStructure>(
      test::GetFormData({.fields = {{.role = USERNAME}, {.role = PASSWORD}}})));

  for (auto& form_structure : form_structures) {
    SetCorrectFieldHostFormSignatures(*form_structure);
  }

  auto crowdsourcing_manager =
      AutofillCrowdsourcingManagerTestApi::CreateManagerForApiKey(&client(),
                                                                  "dummykey");

  // Request with id 0.
  base::HistogramTester histogram;
  EXPECT_TRUE(crowdsourcing_manager->StartQueryRequest(
      ToRawPointerVector(form_structures), driver().GetIsolationInfo(),
      base::BindOnce(
          &AutofillCrowdsourcingManagerTest::OnLoadedServerPredictions,
          GetWeakPtr())));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 1);
  histogram.ExpectUniqueSample(AutofillCrowdsourcingManager::kUmaMethod,
                               METHOD_GET, 1);

  // Validate if the API key is in the request headers.
  network::TestURLLoaderFactory::PendingRequest* request =
      url_loader_factory().GetPendingRequest(0);
  EXPECT_EQ(google_apis::test_util::GetAPIKeyFromRequest(request->request),
            "dummykey");

  // Request with id 1.
  std::vector<AutofillUploadContents> upload_contents_1 = EncodeUploadRequest(
      *form_structures[0], FieldTypeSet(), std::string(), true);
  EXPECT_TRUE(crowdsourcing_manager->StartUploadRequest(
      std::move(upload_contents_1), form_structures[0]->submission_source(),
      /*is_password_manager_upload=*/false));

  // Request with id 2.
  std::vector<AutofillUploadContents> upload_contents_2 = EncodeUploadRequest(
      *form_structures[1], FieldTypeSet(), std::string(), true);
  EXPECT_TRUE(crowdsourcing_manager->StartUploadRequest(
      std::move(upload_contents_2), form_structures[1]->submission_source(),
      /*is_password_manager_upload=*/false));
  // Request with id 3. Upload request with a non-empty additional password form
  // signature.
  std::vector<AutofillUploadContents> upload_contents_3 =
      EncodeUploadRequest(*form_structures[2], FieldTypeSet(), "42", true);
  EXPECT_TRUE(crowdsourcing_manager->StartUploadRequest(
      std::move(upload_contents_3), form_structures[1]->submission_source(),
      /*is_password_manager_upload=*/false));

  // Server responseses - returned  out of sequence.
  const char* response_contents[] = {
      "<autofillqueryresponse>"
      "<field autofilltype=\"86\" />"
      "<field autofilltype=\"3\" />"
      "<field autofilltype=\"5\" />"
      "<field autofilltype=\"9\" />"
      "<field autofilltype=\"9\" />"
      "<field autofilltype=\"75\" />"
      "</autofillqueryresponse>",
      "",
      "<html></html>",
  };

  // Request 1: Successful upload.
  request = url_loader_factory().GetPendingRequest(1);
  url_loader_factory().SimulateResponseWithoutRemovingFromPendingList(
      request, response_contents[1]);
  histogram.ExpectBucketCount("Autofill.Upload.HttpResponseOrErrorCode",
                              net::HTTP_OK, 1);

  // Request 2: Unsuccessful upload.
  request = url_loader_factory().GetPendingRequest(2);
  url_loader_factory().SimulateResponseWithoutRemovingFromPendingList(
      request, network::CreateURLResponseHead(net::HTTP_NOT_FOUND),
      response_contents[2], network::URLLoaderCompletionStatus(net::OK));
  histogram.ExpectBucketCount("Autofill.Upload.HttpResponseOrErrorCode",
                              net::HTTP_NOT_FOUND, 1);

  // Request 0: Successful query.
  request = url_loader_factory().GetPendingRequest(0);
  url_loader_factory().SimulateResponseWithoutRemovingFromPendingList(
      request, response_contents[0]);
  EXPECT_THAT(responses(), SizeIs(1));
  histogram.ExpectBucketCount(AutofillCrowdsourcingManager::kUmaWasInCache,
                              CACHE_MISS, 1);
  histogram.ExpectBucketCount("Autofill.Query.HttpResponseOrErrorCode",
                              net::HTTP_OK, 1);

  // Check Request 0.
  EXPECT_EQ(responses().front().type_of_response,
            ResponseType::kQuerySuccessful);
  EXPECT_EQ(std::string(), responses().front().signature);
  EXPECT_EQ(response_contents[0], responses().front().response);
  responses().pop_front();

  // Add a new form structure that is not in the cache.
  form_structures.push_back(std::make_unique<FormStructure>(test::GetFormData(
      {.fields = {
           {.role = USERNAME}, {.role = PASSWORD}, {.role = PASSWORD}}})));

  // Request with id 4, not successful.
  EXPECT_TRUE(crowdsourcing_manager->StartQueryRequest(
      ToRawPointerVector(form_structures), driver().GetIsolationInfo(),
      base::BindOnce(
          &AutofillCrowdsourcingManagerTest::OnLoadedServerPredictions,
          GetWeakPtr())));
  request = url_loader_factory().GetPendingRequest(4);
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 2);
  histogram.ExpectUniqueSample(AutofillCrowdsourcingManager::kUmaMethod,
                               METHOD_GET, 2);
  url_loader_factory().SimulateResponseWithoutRemovingFromPendingList(
      request, network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR),
      response_contents[0], network::URLLoaderCompletionStatus(net::OK));
  histogram.ExpectBucketCount("Autofill.Query.HttpResponseOrErrorCode",
                              net::HTTP_INTERNAL_SERVER_ERROR, 1);

  // Request with id 5. Let's pretend we hit the cache.
  EXPECT_TRUE(crowdsourcing_manager->StartQueryRequest(
      ToRawPointerVector(form_structures), driver().GetIsolationInfo(),
      base::BindOnce(
          &AutofillCrowdsourcingManagerTest::OnLoadedServerPredictions,
          GetWeakPtr())));
  histogram.ExpectBucketCount("Autofill.ServerQueryResponse",
                              AutofillMetrics::QUERY_SENT, 3);
  histogram.ExpectBucketCount(AutofillCrowdsourcingManager::kUmaMethod,
                              METHOD_GET, 3);
  request = url_loader_factory().GetPendingRequest(5);

  network::URLLoaderCompletionStatus status(net::OK);
  status.exists_in_cache = true;
  url_loader_factory().SimulateResponseWithoutRemovingFromPendingList(
      request, network::CreateURLResponseHead(net::HTTP_OK),
      response_contents[0], status);

  // Check Request 5.
  EXPECT_EQ(responses().front().type_of_response,
            ResponseType::kQuerySuccessful);
  responses().pop_front();
  histogram.ExpectBucketCount(AutofillCrowdsourcingManager::kUmaWasInCache,
                              CACHE_HIT, 1);
}

TEST_F(AutofillCrowdsourcingManagerTest, QueryAPITest) {
  // Build the form structures that we want to query.
  std::vector<std::unique_ptr<FormStructure>> form_structures;
  form_structures.push_back(std::make_unique<FormStructure>(test::GetFormData(
      {.fields = {{.role = NAME_FIRST}, {.role = NAME_LAST}}})));

  auto crowdsourcing_manager =
      AutofillCrowdsourcingManagerTestApi::CreateManagerForApiKey(&client(),
                                                                  "dummykey");

  // Start the query and check its success. No response has been received yet.
  base::HistogramTester histogram;
  EXPECT_TRUE(crowdsourcing_manager->StartQueryRequest(
      ToRawPointerVector(form_structures), driver().GetIsolationInfo(),
      base::BindOnce(
          &AutofillCrowdsourcingManagerTest::OnLoadedServerPredictions,
          GetWeakPtr())));

  // Verify if histograms are right.
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 1);
  histogram.ExpectUniqueSample(AutofillCrowdsourcingManager::kUmaMethod,
                               METHOD_GET, 1);
  {
    auto buckets =
        histogram.GetAllSamples(AutofillCrowdsourcingManager::kUmaGetUrlLength);
    ASSERT_EQ(1U, buckets.size());
    EXPECT_GT(buckets[0].count, 0);
  }
  histogram.ExpectUniqueSample(
      AutofillCrowdsourcingManager::kUmaApiUrlIsTooLong, false, 1);

  // Inspect the request that the test URL loader sent.
  network::TestURLLoaderFactory::PendingRequest* request =
      url_loader_factory().GetPendingRequest(0);

  // Verify request URL and the data payload it carries.
  {
    // This is the URL we expect to query the API. The sub-path right after
    // "/page" corresponds to the serialized AutofillPageQueryRequest proto
    // (that we filled forms in) encoded in base64. The Autofill
    // https://content-autofill.googleapis.com/ domain URL corresponds to the
    // default domain used by the download manager, which is invalid, but good
    // for testing.
    const std::string expected_url =
        R"(https://content-autofill.googleapis.com/v1/pages/(.+)\?alt=proto)";
    std::string encoded_request;
    ASSERT_TRUE(re2::RE2::FullMatch(request->request.url.spec(), expected_url,
                                    &encoded_request));
    AutofillPageQueryRequest request_content;
    ASSERT_TRUE(
        DeserializeAutofillPageQueryRequest(encoded_request, &request_content));
    // Verify form content.
    ASSERT_EQ(request_content.forms().size(), 1);
    EXPECT_EQ(FormSignature(request_content.forms(0).signature()),
              form_structures[0]->form_signature());
    // Verify field content.
    ASSERT_EQ(request_content.forms(0).fields().size(), 2);
    EXPECT_EQ(FieldSignature(request_content.forms(0).fields(0).signature()),
              form_structures[0]->field(0)->GetFieldSignature());
    EXPECT_EQ(FieldSignature(request_content.forms(0).fields(1).signature()),
              form_structures[0]->field(1)->GetFieldSignature());
  }

  // Verify API key header.
  EXPECT_EQ(google_apis::test_util::GetAPIKeyFromRequest(request->request),
            "dummykey");
  // Verify binary response header.
  EXPECT_EQ(request->request.headers.GetHeader(
                "X-Goog-Encode-Response-If-Executable"),
            "base64");

  // Verify response.
  url_loader_factory().SimulateResponseWithoutRemovingFromPendingList(
      request, "dummy response");
  // Upon reception of a suggestions query, we expect OnLoadedServerPredictions
  // to be called back from the observer and some histograms be incremented.
  EXPECT_EQ(1U, responses().size());
  EXPECT_EQ(responses().front().type_of_response,
            ResponseType::kQuerySuccessful);
  histogram.ExpectBucketCount(AutofillCrowdsourcingManager::kUmaWasInCache,
                              CACHE_MISS, 1);
  histogram.ExpectBucketCount("Autofill.Query.HttpResponseOrErrorCode",
                              net::HTTP_OK, 1);
}

TEST_F(AutofillCrowdsourcingManagerTest, QueryAPITestWhenTooLongUrl) {
  // Build the form structures that we want to query.
  std::vector<std::unique_ptr<FormStructure>> form_structures;
  form_structures.emplace_back(std::make_unique<FormStructure>(
      test::GetFormData({.fields = {{.role = NAME_FIRST}}})));

  AutofillCrowdsourcingManagerWithCustomPayloadSize crowdsourcing_manager(
      &client(), "dummykey", kMaxQueryGetSize + 1);

  // Start the query request and look if it is successful. No response was
  // received yet.
  base::HistogramTester histogram;
  EXPECT_TRUE(crowdsourcing_manager.StartQueryRequest(
      ToRawPointerVector(form_structures), driver().GetIsolationInfo(),
      base::BindOnce(
          &AutofillCrowdsourcingManagerTest::OnLoadedServerPredictions,
          GetWeakPtr())));

  // Verify request.
  // Verify if histograms are right.
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 1);
  // Verify that the logged method is POST.
  histogram.ExpectUniqueSample(AutofillCrowdsourcingManager::kUmaMethod,
                               METHOD_POST, 1);
  // Verify that too long URL is tracked.
  histogram.ExpectUniqueSample(
      AutofillCrowdsourcingManager::kUmaApiUrlIsTooLong, true, 1);

  // Get the latest request that the test URL loader sent.
  network::TestURLLoaderFactory::PendingRequest* request =
      url_loader_factory().GetPendingRequest(0);
  // Verify that the POST URL is used when request data too large.
  const std::string expected_url = {
      "https://content-autofill.googleapis.com/v1/pages:get?alt=proto"};
  // Verify API key header.
  EXPECT_EQ(request->request.url, expected_url);
  EXPECT_EQ(google_apis::test_util::GetAPIKeyFromRequest(request->request),
            "dummykey");
  // Verify Content-Type header.
  EXPECT_EQ(request->request.headers.GetHeader("Content-Type"),
            "application/x-protobuf");
  // Verify binary response header.
  EXPECT_EQ(request->request.headers.GetHeader(
                "X-Goog-Encode-Response-If-Executable"),
            "base64");
  // Verify content of the POST body data.
  {
    AutofillPageResourceQueryRequest query_request;
    ASSERT_TRUE(GetAutofillPageResourceQueryRequestFromRequest(request,
                                                               &query_request));
    AutofillPageQueryRequest request_content;
    ASSERT_TRUE(DeserializeAutofillPageQueryRequest(
        query_request.serialized_request(), &request_content));
    // Verify form content.
    ASSERT_EQ(request_content.forms().size(), 1);
    EXPECT_EQ(FormSignature(request_content.forms(0).signature()),
              form_structures[0]->form_signature());
    // Verify field content.
    ASSERT_EQ(request_content.forms(0).fields().size(), 1);
    EXPECT_EQ(FieldSignature(request_content.forms(0).fields(0).signature()),
              form_structures[0]->field(0)->GetFieldSignature());
  }

  // Verify response.
  url_loader_factory().SimulateResponseWithoutRemovingFromPendingList(
      request, "dummy response");
  // Upon reception of a suggestions query, we expect OnLoadedServerPredictions
  // to be called back from the observer and some histograms be incremented.
  EXPECT_EQ(1U, responses().size());
  EXPECT_EQ(responses().front().type_of_response,
            ResponseType::kQuerySuccessful);
  histogram.ExpectBucketCount(AutofillCrowdsourcingManager::kUmaWasInCache,
                              CACHE_MISS, 1);
  histogram.ExpectBucketCount("Autofill.Query.HttpResponseOrErrorCode",
                              net::HTTP_OK, 1);
}

// Test whether uploading vote content to the API is done right. We only do some
// spot checks. No thorough testing is done here. Using the API does not add new
// upload logic.
//
// We expect the download manager to do the following things:
//   * Use the right API canonical URL when uploading.
//   * Serialize the upload proto content using the API upload request proto.
TEST_F(AutofillCrowdsourcingManagerTest, UploadToAPITest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // Enabled
      {},
      // Disabled
      // We don't want upload throttling for testing purpose.
      {features::test::kAutofillUploadThrottling});

  // Build the form structures that we want to upload.
  FormStructure form_structure(test::GetFormData(
      {.fields = {{.role = NAME_FIRST}, {.role = NAME_LAST}}}));
  form_structure.set_submission_source(SubmissionSource::FORM_SUBMISSION);
  SetCorrectFieldHostFormSignatures(form_structure);

  auto crowdsourcing_manager =
      AutofillCrowdsourcingManagerTestApi::CreateManagerForApiKey(&client(),
                                                                  "dummykey");

  std::vector<AutofillUploadContents> upload_contents =
      EncodeUploadRequest(form_structure, FieldTypeSet(), std::string(), true);
  EXPECT_TRUE(crowdsourcing_manager->StartUploadRequest(
      std::move(upload_contents), form_structure.submission_source(),
      /*is_password_manager_upload=*/false));

  // Inspect the request that the test URL loader sent.
  network::TestURLLoaderFactory::PendingRequest* request =
      url_loader_factory().GetPendingRequest(0);

  // This is the URL we expect to upload votes to the API. The Autofill
  // https://content-autofill.googleapis.com/ domain URL corresponds to the
  // default one used by the download manager. Request upload data is in the
  // payload when uploading.
  const std::string expected_url =
      "https://content-autofill.googleapis.com/v1/forms:vote?alt=proto";
  EXPECT_EQ(request->request.url, expected_url);
  EXPECT_EQ(google_apis::test_util::GetAPIKeyFromRequest(request->request),
            "dummykey");

  // Assert some of the fields within the uploaded proto to make sure it was
  // filled with something else than default data.
  base::HistogramTester histogram;
  AutofillUploadRequest upload_request;
  EXPECT_TRUE(GetUploadRequestProtoFromRequest(request, &upload_request));
  EXPECT_GT(upload_request.upload().client_version().size(), 0U);
  EXPECT_EQ(FormSignature(upload_request.upload().form_signature()),
            form_structure.form_signature());

  // Trigger an upload response from the API and assert upload response content.
  url_loader_factory().SimulateResponseWithoutRemovingFromPendingList(request,
                                                                      "");
  histogram.ExpectBucketCount("Autofill.Upload.HttpResponseOrErrorCode",
                              net::HTTP_OK, 1);
}

TEST_F(AutofillCrowdsourcingManagerTest, BackoffLogic_Query) {
  std::vector<std::unique_ptr<FormStructure>> form_structures;
  form_structures.push_back(std::make_unique<FormStructure>(
      test::GetFormData({.fields = {{.role = ADDRESS_HOME_LINE1},
                                    {.role = ADDRESS_HOME_LINE2},
                                    {.role = ADDRESS_HOME_CITY}}})));
  for (auto& form_structure : form_structures) {
    SetCorrectFieldHostFormSignatures(*form_structure);
  }

  // Request with id 0.
  base::HistogramTester histogram;
  EXPECT_TRUE(StartQueryRequest(form_structures));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 1);

  auto* request = url_loader_factory().GetPendingRequest(0);

  url_loader_factory().SimulateResponseWithoutRemovingFromPendingList(
      request, network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR),
      "", network::URLLoaderCompletionStatus(net::OK));
  EXPECT_THAT(responses(), IsEmpty());

  // A request error incurs a retry after 1 second (+- 33% fuzzing).
  EXPECT_EQ(url_loader_factory().GetPendingRequest(1), nullptr);
  task_environment().FastForwardBy(base::Milliseconds(1400));
  ASSERT_NE(url_loader_factory().GetPendingRequest(1), nullptr);

  // Get the retried request.
  request = url_loader_factory().GetPendingRequest(1);
  url_loader_factory().SimulateResponseWithoutRemovingFromPendingList(
      request,
      network::CreateURLResponseHead(net::HTTP_REQUEST_ENTITY_TOO_LARGE),
      "<html></html>", network::URLLoaderCompletionStatus(net::OK));
  EXPECT_THAT(responses(), IsEmpty());

  // No more retries occur because the error was a client error.
  task_environment().FastForwardBy(base::Milliseconds(3000));
  EXPECT_EQ(url_loader_factory().GetPendingRequest(2), nullptr);

  histogram.ExpectBucketCount("Autofill.Query.HttpResponseOrErrorCode",
                              net::HTTP_REQUEST_ENTITY_TOO_LARGE, 1);
  auto buckets = histogram.GetAllSamples("Autofill.Query.FailingPayloadSize");
  ASSERT_THAT(buckets, SizeIs(1));
  EXPECT_EQ(2, buckets[0].count);
}

TEST_F(AutofillCrowdsourcingManagerTest, BackoffLogic_Upload) {
  FormStructure form_structure(
      test::GetFormData({.fields = {{.role = ADDRESS_HOME_LINE1},
                                    {.role = ADDRESS_HOME_LINE2},
                                    {.role = ADDRESS_HOME_CITY}}}));
  form_structure.set_submission_source(SubmissionSource::FORM_SUBMISSION);
  SetCorrectFieldHostFormSignatures(form_structure);

  // Request with id 0.
  std::vector<AutofillUploadContents> upload_contents =
      EncodeUploadRequest(form_structure, FieldTypeSet(), std::string(), true);
  EXPECT_TRUE(crowdsourcing_manager().StartUploadRequest(
      std::move(upload_contents), form_structure.submission_source(),
      /*is_password_manager_upload=*/false));

  auto* request = url_loader_factory().GetPendingRequest(0);

  url_loader_factory().SimulateResponseWithoutRemovingFromPendingList(
      request, network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR),
      "", network::URLLoaderCompletionStatus(net::OK));
  EXPECT_THAT(responses(), IsEmpty());

  // A request error incurs a retry after 1 second (+- 33% fuzzing).
  EXPECT_EQ(url_loader_factory().GetPendingRequest(1), nullptr);
  task_environment().FastForwardBy(base::Milliseconds(1400));
  ASSERT_NE(url_loader_factory().GetPendingRequest(1), nullptr);

  // Get the retried request, and make it successful.
  request = url_loader_factory().GetPendingRequest(1);
  url_loader_factory().SimulateResponseWithoutRemovingFromPendingList(request,
                                                                      "");
  // Validate that there is no retry on sending a bad request.
  form_structure.set_submission_source(SubmissionSource::XHR_SUCCEEDED);
  base::HistogramTester histogram;
  std::vector<AutofillUploadContents> upload_contents_2 =
      EncodeUploadRequest(form_structure, FieldTypeSet(), std::string(), true);
  EXPECT_TRUE(crowdsourcing_manager().StartUploadRequest(
      std::move(upload_contents_2), form_structure.submission_source(),
      /*is_password_manager_upload=*/false));

  request = url_loader_factory().GetPendingRequest(2);
  url_loader_factory().SimulateResponseWithoutRemovingFromPendingList(
      request,
      network::CreateURLResponseHead(net::HTTP_REQUEST_ENTITY_TOO_LARGE), "",
      network::URLLoaderCompletionStatus(net::OK));
  ASSERT_EQ(url_loader_factory().NumPending(), 0);
  histogram.ExpectBucketCount("Autofill.Upload.HttpResponseOrErrorCode",
                              net::HTTP_REQUEST_ENTITY_TOO_LARGE, 1);
  auto buckets = histogram.GetAllSamples("Autofill.Upload.FailingPayloadSize");
  ASSERT_THAT(buckets, SizeIs(1));
  EXPECT_EQ(1, buckets[0].count);
}

TEST_F(AutofillCrowdsourcingManagerTest, RetryLimit_Query) {
  std::vector<std::unique_ptr<FormStructure>> form_structures;
  form_structures.push_back(std::make_unique<FormStructure>(
      test::GetFormData({.fields = {{.role = ADDRESS_HOME_LINE1},
                                    {.role = ADDRESS_HOME_LINE2},
                                    {.role = ADDRESS_HOME_CITY}}})));

  // Request with id 0.
  base::HistogramTester histogram;
  EXPECT_TRUE(StartQueryRequest(form_structures));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 1);

  constexpr auto kTimeDeltaMargin = base::Milliseconds(100);
  const int max_attempts = crowdsourcing_manager().GetMaxServerAttempts();
  for (int attempt = 0; attempt < max_attempts; ++attempt) {
    auto* request = url_loader_factory().GetPendingRequest(attempt);
    ASSERT_TRUE(request != nullptr);

    // Request error incurs a retry after 1 second.
    url_loader_factory().SimulateResponseWithoutRemovingFromPendingList(
        request,
        network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR),
        "<html></html>", network::URLLoaderCompletionStatus(net::OK));

    // There should be no response in case of an error.
    EXPECT_THAT(responses(), IsEmpty());

    task_environment().FastForwardBy(
        test_api(crowdsourcing_manager()).GetCurrentBackoffTime() +
        kTimeDeltaMargin);
  }

  // There should not be an additional retry.
  EXPECT_EQ(nullptr, url_loader_factory().GetPendingRequest(max_attempts));
  EXPECT_EQ(url_loader_factory().NumPending(), 0);

  // Verify metrics.
  histogram.ExpectBucketCount("Autofill.Query.HttpResponseOrErrorCode",
                              net::HTTP_INTERNAL_SERVER_ERROR, max_attempts);
  auto buckets = histogram.GetAllSamples("Autofill.Query.FailingPayloadSize");
  ASSERT_THAT(buckets, SizeIs(1));
  EXPECT_EQ(max_attempts, buckets[0].count);
}

TEST_F(AutofillCrowdsourcingManagerTest, RetryLimit_Upload) {
  base::HistogramTester histogram;
  FormStructure form_structure(
      test::GetFormData({.fields = {{.role = ADDRESS_HOME_LINE1},
                                    {.role = ADDRESS_HOME_LINE2},
                                    {.role = ADDRESS_HOME_CITY}}}));
  form_structure.set_submission_source(SubmissionSource::FORM_SUBMISSION);
  SetCorrectFieldHostFormSignatures(form_structure);

  // Request with id 0.
  std::vector<AutofillUploadContents> upload_contents =
      EncodeUploadRequest(form_structure, FieldTypeSet(), std::string(), true);
  EXPECT_TRUE(crowdsourcing_manager().StartUploadRequest(
      std::move(upload_contents), form_structure.submission_source(),
      /*is_password_manager_upload=*/false));

  constexpr auto kTimeDeltaMargin = base::Milliseconds(100);
  const int max_attempts = crowdsourcing_manager().GetMaxServerAttempts();
  for (int attempt = 0; attempt < max_attempts; ++attempt) {
    auto* request = url_loader_factory().GetPendingRequest(attempt);
    ASSERT_NE(request, nullptr);

    // Simulate a server failure.
    url_loader_factory().SimulateResponseWithoutRemovingFromPendingList(
        request,
        network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR), "",
        network::URLLoaderCompletionStatus(net::OK));
    EXPECT_THAT(responses(), IsEmpty());

    task_environment().FastForwardBy(
        test_api(crowdsourcing_manager()).GetCurrentBackoffTime() +
        kTimeDeltaMargin);
  }

  // There should not be an additional retry.
  EXPECT_EQ(nullptr, url_loader_factory().GetPendingRequest(max_attempts));
  EXPECT_EQ(url_loader_factory().NumPending(), 0);

  // Verify metrics.
  histogram.ExpectBucketCount("Autofill.Upload.HttpResponseOrErrorCode",
                              net::HTTP_INTERNAL_SERVER_ERROR, max_attempts);
  auto buckets = histogram.GetAllSamples("Autofill.Upload.FailingPayloadSize");
  ASSERT_THAT(buckets, SizeIs(1));
  EXPECT_EQ(max_attempts, buckets[0].count);
}

TEST_F(AutofillCrowdsourcingManagerTest, QueryTooManyFieldsTest) {
  // Create a query that contains too many fields for the server.
  std::vector<FormData> forms(21);
  std::vector<std::unique_ptr<FormStructure>> form_structures;
  for (auto& form : forms) {
    for (size_t i = 0; i < 5; ++i) {
      test_api(form).Append(CreateTestFormField(base::NumberToString(i),
                                                base::NumberToString(i), "",
                                                FormControlType::kInputText));
    }
    form_structures.push_back(std::make_unique<FormStructure>(form));
  }

  // Check whether the query is aborted.
  EXPECT_FALSE(StartQueryRequest(form_structures));
}

TEST_F(AutofillCrowdsourcingManagerTest, QueryNotTooManyFieldsTest) {
  // Create a query that contains a lot of fields, but not too many for the
  // server.
  std::vector<FormData> forms(25);
  std::vector<std::unique_ptr<FormStructure>> form_structures;
  for (auto& form : forms) {
    for (size_t i = 0; i < 4; ++i) {
      test_api(form).Append(CreateTestFormField(base::NumberToString(i),
                                                base::NumberToString(i), "",
                                                FormControlType::kInputText));
    }
    form_structures.push_back(std::make_unique<FormStructure>(form));
  }

  // Check that the query is not aborted.
  EXPECT_TRUE(StartQueryRequest(form_structures));
}

TEST_F(AutofillCrowdsourcingManagerTest, CacheQueryTest) {
  std::vector<std::unique_ptr<FormStructure>> form_structures0;
  form_structures0.push_back(std::make_unique<FormStructure>(test::GetFormData(
      {.fields = {
           {.role = USERNAME}, {.role = NAME_FIRST}, {.role = NAME_LAST}}})));

  // Make a slightly different form, which should result in a different request.
  std::vector<std::unique_ptr<FormStructure>> form_structures1;
  form_structures1.push_back(std::make_unique<FormStructure>(
      test::GetFormData({.fields = {{.role = USERNAME},
                                    {.role = NAME_FIRST},
                                    {.role = NAME_LAST},
                                    {.role = EMAIL_ADDRESS}}})));

  // Make yet another slightly different form, which should also result in a
  // different request.
  std::vector<std::unique_ptr<FormStructure>> form_structures2;
  form_structures2.push_back(std::make_unique<FormStructure>(
      test::GetFormData({.fields = {{.role = USERNAME},
                                    {.role = NAME_FIRST},
                                    {.role = NAME_LAST},
                                    {.role = EMAIL_ADDRESS},
                                    {.role = EMAIL_ADDRESS}}})));

  test_api(crowdsourcing_manager()).set_max_form_cache_size(2);

  const char* response_contents[] = {
      "<autofillqueryresponse>"
      "<field autofilltype=\"0\" />"
      "<field autofilltype=\"3\" />"
      "<field autofilltype=\"5\" />"
      "</autofillqueryresponse>",
      "<autofillqueryresponse>"
      "<field autofilltype=\"0\" />"
      "<field autofilltype=\"3\" />"
      "<field autofilltype=\"5\" />"
      "<field autofilltype=\"9\" />"
      "</autofillqueryresponse>",
      "<autofillqueryresponse>"
      "<field autofilltype=\"0\" />"
      "<field autofilltype=\"3\" />"
      "<field autofilltype=\"5\" />"
      "<field autofilltype=\"9\" />"
      "<field autofilltype=\"0\" />"
      "</autofillqueryresponse>",
  };

  base::HistogramTester histogram;
  // Request with id 0.
  EXPECT_TRUE(StartQueryRequest(form_structures0));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 1);

  // No responses yet
  EXPECT_THAT(responses(), IsEmpty());

  auto* request = url_loader_factory().GetPendingRequest(0);
  url_loader_factory().SimulateResponseWithoutRemovingFromPendingList(
      request, response_contents[0]);
  ASSERT_THAT(responses(), SizeIs(1));
  EXPECT_EQ(response_contents[0], responses().front().response);

  responses().clear();

  // No actual request - should be a cache hit.
  EXPECT_TRUE(StartQueryRequest(form_structures0));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 2);
  // Data is available immediately from cache - no over-the-wire trip.
  ASSERT_THAT(responses(), SizeIs(1));
  EXPECT_EQ(response_contents[0], responses().front().response);
  responses().clear();

  // Request with id 1.
  EXPECT_TRUE(StartQueryRequest(form_structures1));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 3);
  // No responses yet
  EXPECT_THAT(responses(), IsEmpty());

  request = url_loader_factory().GetPendingRequest(1);
  url_loader_factory().SimulateResponseWithoutRemovingFromPendingList(
      request, response_contents[1]);
  ASSERT_THAT(responses(), SizeIs(1));
  EXPECT_EQ(response_contents[1], responses().front().response);

  responses().clear();

  // Request with id 2.
  EXPECT_TRUE(StartQueryRequest(form_structures2));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 4);

  request = url_loader_factory().GetPendingRequest(2);
  url_loader_factory().SimulateResponseWithoutRemovingFromPendingList(
      request, response_contents[2]);
  ASSERT_THAT(responses(), SizeIs(1));
  EXPECT_EQ(response_contents[2], responses().front().response);

  responses().clear();

  // No actual requests - should be a cache hit.
  EXPECT_TRUE(StartQueryRequest(form_structures1));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 5);

  EXPECT_TRUE(StartQueryRequest(form_structures2));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 6);

  ASSERT_THAT(responses(), SizeIs(2));
  EXPECT_EQ(response_contents[1], responses().front().response);
  EXPECT_EQ(response_contents[2], responses().back().response);
  responses().clear();

  // The first structure should have expired.
  // Request with id 3.
  EXPECT_TRUE(StartQueryRequest(form_structures0));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 7);
  // No responses yet
  EXPECT_THAT(responses(), IsEmpty());

  request = url_loader_factory().GetPendingRequest(3);
  url_loader_factory().SimulateResponseWithoutRemovingFromPendingList(
      request, response_contents[0]);
  ASSERT_THAT(responses(), SizeIs(1));
  EXPECT_EQ(response_contents[0], responses().front().response);
}

namespace {

enum ServerCommunicationMode {
  DISABLED,
  FINCHED_URL,
  COMMAND_LINE_URL,
  DEFAULT_URL
};

class AutofillServerCommunicationTest
    : public testing::TestWithParam<ServerCommunicationMode> {
 protected:
  void SetUp() override {
    testing::TestWithParam<ServerCommunicationMode>::SetUp();

    scoped_feature_list_1_.InitWithFeatures(
        // Enabled
        {features::test::kAutofillUploadThrottling},
        // Disabled
        {});

    // Setup the server.
    server_.RegisterRequestHandler(
        base::BindRepeating(&AutofillServerCommunicationTest::RequestHandler,
                            base::Unretained(this)));
    ASSERT_TRUE(server_.Start());

    GURL autofill_server_url(server_.base_url());
    ASSERT_TRUE(autofill_server_url.is_valid());

    shared_url_loader_factory_ =
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
            nullptr /* network_service */, true /* is_trusted */);
    client_.set_shared_url_loader_factory(shared_url_loader_factory_);
    driver_.SetIsolationInfo(net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther,
        url::Origin::Create(GURL("https://abc.com")),
        url::Origin::Create(GURL("https://xyz.com")), net::SiteForCookies()));

    // Configure the autofill server communications channel.
    switch (GetParam()) {
      case DISABLED:
        scoped_feature_list_2_.InitAndDisableFeature(
            features::test::kAutofillServerCommunication);
        break;
      case FINCHED_URL:
        scoped_feature_list_2_.InitAndEnableFeatureWithParameters(
            features::test::kAutofillServerCommunication,
            {{switches::kAutofillServerURL, autofill_server_url.spec()}});
        break;
      case COMMAND_LINE_URL:
        scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
            switches::kAutofillServerURL, autofill_server_url.spec());
        [[fallthrough]];
      case DEFAULT_URL:
        scoped_feature_list_2_.InitAndEnableFeature(
            features::test::kAutofillServerCommunication);
        break;
      default:
        ASSERT_TRUE(false);
    }
  }

  void TearDown() override {
    if (server_.Started()) {
      ASSERT_TRUE(server_.ShutdownAndWaitUntilComplete());
    }

    auto* variations_ids_provider =
        variations::VariationsIdsProvider::GetInstance();
    if (variations_ids_provider != nullptr) {
      variations_ids_provider->ResetForTesting();
    }
  }

  // AutofillCrowdsourcingManager::Observer implementation.
  void OnLoadedServerPredictions(
      std::optional<AutofillCrowdsourcingManager::QueryResponse> response) {
    ASSERT_TRUE(run_loop_);
    run_loop_->QuitWhenIdle();
  }

  // Helper to extract the value passed to a lookup in the URL. Returns "*** not
  // found ***" if the the data cannot be decoded.
  std::string GetLookupContent(const std::string& query_path) {
    if (query_path.find("/v1/pages/") == std::string::npos) {
      return "*** not found ***";
    }
    std::string payload = query_path.substr(strlen("/v1/pages/"));
    std::string decoded_payload;
    if (base::Base64UrlDecode(payload,
                              base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                              &decoded_payload)) {
      return decoded_payload;
    }
    return "*** not found ***";
  }

  std::unique_ptr<HttpResponse> RequestHandler(const HttpRequest& request) {
    GURL absolute_url = server_.GetURL(request.relative_url);
    ++call_count_;

    if (absolute_url.path().find("/v1/pages") == 0) {
      payloads().push_back(!request.content.empty()
                               ? request.content
                               : GetLookupContent(absolute_url.path()));
      AutofillQueryResponse proto;
      proto.add_form_suggestions()
          ->add_field_suggestions()
          ->add_predictions()
          ->set_type(NAME_FIRST);

      auto response = std::make_unique<BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content(proto.SerializeAsString());
      response->set_content_type("text/proto");
      response->AddCustomHeader(
          "Cache-Control",
          base::StringPrintf("max-age=%" PRId64,
                             cache_expiration_time_.InSeconds()));
      return response;
    }

    if (absolute_url.path() == "/v1/forms:vote") {
      payloads().push_back(request.content);
      auto response = std::make_unique<BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      run_loop_->QuitWhenIdle();
      return response;
    }

    return nullptr;
  }

  bool SendQueryRequest(
      const std::vector<std::unique_ptr<FormStructure>>& form_structures) {
    EXPECT_EQ(run_loop_, nullptr);
    run_loop_ = std::make_unique<base::RunLoop>();

    ScopedActiveAutofillExperiments scoped_active_autofill_experiments;
    AutofillCrowdsourcingManager crowdsourcing_manager(
        &client(), version_info::Channel::UNKNOWN, nullptr);
    bool succeeded = crowdsourcing_manager.StartQueryRequest(
        ToRawPointerVector(form_structures), driver_.GetIsolationInfo(),
        base::BindOnce(
            &AutofillServerCommunicationTest::OnLoadedServerPredictions,
            weak_ptr_factory_.GetWeakPtr()));
    if (succeeded) {
      run_loop_->Run();
    }
    run_loop_.reset();
    return succeeded;
  }

  bool SendUploadRequest(const FormStructure& form,
                         const FieldTypeSet& available_field_types,
                         const std::string& login_form_signature,
                         bool observed_submission,
                         bool is_password_manager_upload) {
    EXPECT_EQ(run_loop_, nullptr);
    run_loop_ = std::make_unique<base::RunLoop>();

    ScopedActiveAutofillExperiments scoped_active_autofill_experiments;
    AutofillCrowdsourcingManager crowdsourcing_manager(
        &client(), version_info::Channel::UNKNOWN, nullptr);

    std::vector<AutofillUploadContents> upload_contents = EncodeUploadRequest(
        form, available_field_types, login_form_signature, observed_submission);
    bool succeeded = crowdsourcing_manager.StartUploadRequest(
        std::move(upload_contents), form.submission_source(),
        is_password_manager_upload);
    if (succeeded) {
      run_loop_->Run();
    }
    run_loop_.reset();
    return succeeded;
  }

  void ResetCallCount() { call_count_ = 0; }

  int call_count() const { return call_count_; }
  TestAutofillClient& client() { return client_; }
  std::vector<std::string>& payloads() { return payloads_; }
  void set_cache_expiration_time(base::TimeDelta time) {
    cache_expiration_time_ = time;
  }

 private:
  test::AutofillUnitTestEnvironment autofill_environment_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  base::test::ScopedCommandLine scoped_command_line_;
  base::test::ScopedFeatureList scoped_feature_list_1_;
  base::test::ScopedFeatureList scoped_feature_list_2_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  EmbeddedTestServer server_;
  std::unique_ptr<base::RunLoop> run_loop_;
  scoped_refptr<network::TestSharedURLLoaderFactory> shared_url_loader_factory_;
  TestAutofillClient client_;
  TestAutofillDriver driver_{&client_};
  base::TimeDelta cache_expiration_time_ = base::Seconds(100);
  int call_count_ = 0;
  std::vector<std::string> payloads_;
  base::WeakPtrFactory<AutofillServerCommunicationTest> weak_ptr_factory_{this};
};

}  // namespace

TEST_P(AutofillServerCommunicationTest, IsEnabled) {
  AutofillCrowdsourcingManager crowdsourcing_manager(
      &client(), version_info::Channel::UNKNOWN, nullptr);
  EXPECT_EQ(crowdsourcing_manager.IsEnabled(), GetParam() != DISABLED);
}

TEST_P(AutofillServerCommunicationTest, Query) {
  std::vector<std::unique_ptr<FormStructure>> form_structures;
  form_structures.push_back(std::make_unique<FormStructure>(
      test::GetFormData({.fields = {{.role = NAME_FIRST}}})));

  EXPECT_EQ(GetParam() != DISABLED, SendQueryRequest(form_structures));
}

TEST_P(AutofillServerCommunicationTest, Upload) {
  AutofillCrowdsourcingManager crowdsourcing_manager(
      &client(), version_info::Channel::UNKNOWN, nullptr);
  EXPECT_EQ(GetParam() != DISABLED,
            SendUploadRequest(
                FormStructure(
                    test::GetFormData({.fields = {{.role = NAME_FIRST},
                                                  {.role = NAME_LAST},
                                                  {.role = EMAIL_ADDRESS}}})),
                /*available_field_types=*/{}, /*login_form_signature=*/"",
                /*observed_submission=*/true,
                /*is_password_manager_upload=*/false));
}

// Note that we omit DEFAULT_URL from the test params. We don't actually want
// the tests to hit the production server.
INSTANTIATE_TEST_SUITE_P(All,
                         AutofillServerCommunicationTest,
                         ::testing::Values(DISABLED,
                                           FINCHED_URL,
                                           COMMAND_LINE_URL));

using AutofillQueryTest = AutofillServerCommunicationTest;

TEST_P(AutofillQueryTest, CacheableResponse) {
  std::vector<std::unique_ptr<FormStructure>> form_structures;
  form_structures.push_back(std::make_unique<FormStructure>(
      test::GetFormData({.fields = {{.role = NAME_FIRST}}})));

  // Query for the form. This should go to the embedded server.
  {
    SCOPED_TRACE("First Query");
    base::HistogramTester histogram;
    ResetCallCount();
    ASSERT_TRUE(SendQueryRequest(form_structures));
    EXPECT_EQ(1, call_count());
    histogram.ExpectBucketCount("Autofill.ServerQueryResponse",
                                AutofillMetrics::QUERY_SENT, 1);
    histogram.ExpectBucketCount(AutofillCrowdsourcingManager::kUmaMethod,
                                METHOD_GET, 1);
    histogram.ExpectBucketCount(AutofillCrowdsourcingManager::kUmaWasInCache,
                                CACHE_MISS, 1);
  }

  // Query again. This should go to the cache, since the max-age for the cached
  // response is 2 days.
  {
    SCOPED_TRACE("Second Query");
    base::HistogramTester histogram;
    ResetCallCount();
    ASSERT_TRUE(SendQueryRequest(form_structures));
    EXPECT_EQ(0, call_count());
    histogram.ExpectBucketCount("Autofill.ServerQueryResponse",
                                AutofillMetrics::QUERY_SENT, 1);
    histogram.ExpectBucketCount(AutofillCrowdsourcingManager::kUmaMethod,
                                METHOD_GET, 1);
    histogram.ExpectBucketCount(AutofillCrowdsourcingManager::kUmaWasInCache,
                                CACHE_HIT, 1);
  }
}

TEST_P(AutofillQueryTest, SendsExperiment) {
  std::vector<std::unique_ptr<FormStructure>> form_structures;
  form_structures.push_back(std::make_unique<FormStructure>(
      test::GetFormData({.fields = {{.role = NAME_FIRST}}})));

  // Query for the form. This should go to the embedded server.
  {
    SCOPED_TRACE("First Query");
    base::HistogramTester histogram;
    ResetCallCount();
    ASSERT_TRUE(SendQueryRequest(form_structures));
    EXPECT_EQ(1, call_count());
    histogram.ExpectBucketCount("Autofill.ServerQueryResponse",
                                AutofillMetrics::QUERY_SENT, 1);
    histogram.ExpectBucketCount(AutofillCrowdsourcingManager::kUmaMethod,
                                METHOD_GET, 1);
    histogram.ExpectBucketCount(AutofillCrowdsourcingManager::kUmaWasInCache,
                                CACHE_MISS, 1);
  }

  // Add experiment/variation idd from the range reserved for autofill.
  auto* variations_ids_provider =
      variations::VariationsIdsProvider::GetInstance();
  ASSERT_TRUE(variations_ids_provider != nullptr);
  variations_ids_provider->ForceVariationIds(
      {"t3314883", "t3312923", "t3314885"},  // first two valid, out-of-order
      {});

  // Query again. This should go to the embedded server since it's not the same
  // as the previously cached query.
  {
    SCOPED_TRACE("Second Query");
    base::HistogramTester histogram;
    ResetCallCount();
    payloads().clear();
    ASSERT_TRUE(SendQueryRequest(form_structures));
    EXPECT_EQ(1, call_count());
    histogram.ExpectBucketCount("Autofill.ServerQueryResponse",
                                AutofillMetrics::QUERY_SENT, 1);
    histogram.ExpectBucketCount(AutofillCrowdsourcingManager::kUmaMethod,
                                METHOD_GET, 1);
    histogram.ExpectBucketCount(AutofillCrowdsourcingManager::kUmaWasInCache,
                                CACHE_MISS, 1);

    ASSERT_THAT(payloads(), SizeIs(1));
    AutofillPageQueryRequest query_contents;
    ASSERT_TRUE(query_contents.ParseFromString(payloads()[0]));

    ASSERT_EQ(2, query_contents.experiments_size());
    EXPECT_EQ(3312923, query_contents.experiments(0));
    EXPECT_EQ(3314883, query_contents.experiments(1));
  }

  // Query a third time (will experiments still enabled). This should go to the
  // cache.
  {
    SCOPED_TRACE("Third Query");
    base::HistogramTester histogram;
    ResetCallCount();
    ASSERT_TRUE(SendQueryRequest(form_structures));
    EXPECT_EQ(0, call_count());
    histogram.ExpectBucketCount("Autofill.ServerQueryResponse",
                                AutofillMetrics::QUERY_SENT, 1);
    histogram.ExpectBucketCount(AutofillCrowdsourcingManager::kUmaMethod,
                                METHOD_GET, 1);
    histogram.ExpectBucketCount(AutofillCrowdsourcingManager::kUmaWasInCache,
                                CACHE_HIT, 1);
  }
}

TEST_P(AutofillQueryTest, ExpiredCacheInResponse) {
  std::vector<std::unique_ptr<FormStructure>> form_structures;
  form_structures.push_back(std::make_unique<FormStructure>(
      test::GetFormData({.fields = {{.role = NAME_FIRST}}})));

  set_cache_expiration_time(base::Seconds(0));

  // Query for the form. This should go to the embedded server.
  {
    SCOPED_TRACE("First Query");
    base::HistogramTester histogram;
    ResetCallCount();
    ASSERT_TRUE(SendQueryRequest(form_structures));
    EXPECT_EQ(1, call_count());
    histogram.ExpectBucketCount("Autofill.ServerQueryResponse",
                                AutofillMetrics::QUERY_SENT, 1);
    histogram.ExpectBucketCount(AutofillCrowdsourcingManager::kUmaMethod,
                                METHOD_GET, 1);
    histogram.ExpectBucketCount(AutofillCrowdsourcingManager::kUmaWasInCache,
                                CACHE_MISS, 1);
  }

  // The cache entry had a max age of 0 ms, so delaying only a few milliseconds
  // ensures the cache expires and no request are served by cached content
  // (ie this should go to the embedded server).
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
  run_loop.Run();

  {
    SCOPED_TRACE("Second Query");
    base::HistogramTester histogram;
    ResetCallCount();
    ASSERT_TRUE(SendQueryRequest(form_structures));
    EXPECT_EQ(1, call_count());
    histogram.ExpectBucketCount("Autofill.ServerQueryResponse",
                                AutofillMetrics::QUERY_SENT, 1);
    histogram.ExpectBucketCount(AutofillCrowdsourcingManager::kUmaMethod,
                                METHOD_GET, 1);
    histogram.ExpectBucketCount(AutofillCrowdsourcingManager::kUmaWasInCache,
                                CACHE_MISS, 1);
  }
}

TEST_P(AutofillQueryTest, Metadata) {
  // Initialize a form. Note that this state is post-parse.
  FormData form;
  form.set_url(GURL("https://origin.com"));
  form.set_action(GURL("https://origin.com/submit-me"));
  form.set_id_attribute(u"form-id-attribute");
  form.set_name_attribute(u"form-name-attribute");
  form.set_name(form.name_attribute());

  // Add field 0.
  FormFieldData field;
  field.set_id_attribute(u"field-id-attribute-1");
  field.set_name_attribute(u"field-name-attribute-1");
  field.set_name(field.name_attribute());
  field.set_label(u"field-label");
  field.set_aria_label(u"field-aria-label");
  field.set_aria_description(u"field-aria-description");
  field.set_form_control_type(FormControlType::kInputText);
  field.set_css_classes(u"field-css-classes");
  field.set_placeholder(u"field-placeholder");
  test_api(form).Append(field);

  // Add field 1.
  field.set_id_attribute(u"field-id-attribute-2");
  field.set_name_attribute(u"field-name-attribute-2");
  field.set_name(field.name_attribute());
  field.set_label(u"field-label");
  field.set_aria_label(u"field-aria-label");
  field.set_aria_description(u"field-aria-description");
  field.set_form_control_type(FormControlType::kInputText);
  field.set_css_classes(u"field-css-classes");
  field.set_placeholder(u"field-placeholder");
  test_api(form).Append(field);

  // Add field 2.
  field.set_id_attribute(u"field-id-attribute-3");
  field.set_name_attribute(u"field-name-attribute-3");
  field.set_name(field.name_attribute());
  field.set_label(u"field-label");
  field.set_aria_label(u"field-aria-label");
  field.set_aria_description(u"field-aria-description");
  field.set_form_control_type(FormControlType::kInputText);
  field.set_css_classes(u"field-css-classes");
  field.set_placeholder(u"field-placeholder");
  test_api(form).Append(field);

  // Setup the form structures to query.
  AutofillCrowdsourcingManager crowdsourcing_manager(
      &client(), version_info::Channel::UNKNOWN, nullptr);
  std::vector<std::unique_ptr<FormStructure>> form_structures;
  form_structures.push_back(std::make_unique<FormStructure>(form));

  // Generate a query request.
  ASSERT_TRUE(SendQueryRequest(form_structures));
  EXPECT_EQ(1, call_count());

  // We should have intercepted exactly on query request. Parse it.
  ASSERT_THAT(payloads(), SizeIs(1));
  AutofillPageQueryRequest query;
  ASSERT_TRUE(query.ParseFromString(payloads().front()));

  // Validate that we have one form in the query.
  ASSERT_EQ(query.forms_size(), 1);
  const auto& query_form = query.forms(0);

  // There should be no encoded metadata for the form.
  EXPECT_FALSE(query_form.has_metadata());

  // There should be three fields, none of which have encoded metadata.
  ASSERT_EQ(3, query_form.fields_size());
  ASSERT_EQ(static_cast<int>(form.fields().size()), query_form.fields_size());
  for (int i = 0; i < query_form.fields_size(); ++i) {
    const auto& query_field = query_form.fields(i);
    EXPECT_FALSE(query_field.has_metadata());
  }
}

// Note that we omit DEFAULT_URL from the test params. We don't actually want
// the tests to hit the production server. We also excluded DISABLED, since
// these tests exercise "enabled" functionality.
INSTANTIATE_TEST_SUITE_P(All,
                         AutofillQueryTest,
                         ::testing::Values(FINCHED_URL, COMMAND_LINE_URL));

using AutofillUploadTest = AutofillServerCommunicationTest;

TEST_P(AutofillUploadTest, RichMetadata) {
  base::test::ScopedFeatureList local_feature;

  FormData form;
  form.set_url(GURL("https://origin.com"));
  form.set_full_url(GURL("https://origin.com?foo=bar#foo"));
  form.set_action(GURL("https://origin.com/submit-me"));
  form.set_id_attribute(u"form-id_attribute");
  form.set_name_attribute(u"form-id_attribute");
  form.set_name(form.name_attribute());

  FormFieldData field;
  field.set_id_attribute(u"field-id-attribute-1");
  field.set_name_attribute(u"field-name-attribute-1");
  field.set_name(field.name_attribute());
  field.set_label(u"field-label");
  field.set_aria_label(u"field-aria-label");
  field.set_aria_description(u"field-aria-descriptionm");
  field.set_form_control_type(FormControlType::kInputText);
  field.set_css_classes(u"field-css-classes");
  field.set_placeholder(u"field-placeholder");
  test_api(form).Append(field);

  field.set_id_attribute(u"field-id-attribute-2");
  field.set_name_attribute(u"field-name-attribute-2");
  field.set_name(field.name_attribute());
  field.set_label(u"field-label");
  field.set_aria_label(u"field-aria-label");
  field.set_aria_description(u"field-aria-descriptionm");
  field.set_form_control_type(FormControlType::kInputText);
  field.set_css_classes(u"field-css-classes");
  field.set_placeholder(u"field-placeholder");
  test_api(form).Append(field);

  field.set_id_attribute(u"field-id-attribute-3");
  field.set_name_attribute(u"field-name-attribute-3");
  field.set_name(field.name_attribute());
  field.set_label(u"field-label");
  field.set_aria_label(u"field-aria-label");
  field.set_aria_description(u"field-aria-descriptionm");
  field.set_form_control_type(FormControlType::kInputText);
  field.set_css_classes(u"field-css-classes");
  field.set_placeholder(u"field-placeholder");
  test_api(form).Append(field);

  AutofillCrowdsourcingManager crowdsourcing_manager(
      &client(), version_info::Channel::UNKNOWN, nullptr);
  FormStructure form_structure(form);
  form_structure.set_current_page_language(LanguageCode("fr"));
  SetCorrectFieldHostFormSignatures(form_structure);

  client().GetPrefs()->SetBoolean(
      RandomizedEncoder::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  for (int i = 0; i <= static_cast<int>(SubmissionSource::kMaxValue); ++i) {
    base::HistogramTester histogram_tester;
    auto submission_source = static_cast<SubmissionSource>(i);
    SCOPED_TRACE(testing::Message()
                 << "submission source = " << submission_source);
    form_structure.set_submission_source(submission_source);
    form_structure.set_randomized_encoder(
        RandomizedEncoder::Create(client().GetPrefs()));

    payloads().clear();

    // The first attempt should succeed.
    EXPECT_TRUE(SendUploadRequest(form_structure, /*available_field_types=*/{},
                                  /*login_form_signature=*/"",
                                  /*observed_submission=*/true,
                                  /*is_password_manager_upload=*/false));

    // The second attempt should always fail.
    EXPECT_FALSE(SendUploadRequest(form_structure, /*available_field_types=*/{},
                                   /*login_form_signature=*/"",
                                   /*observed_submission=*/true,
                                   /*is_password_manager_upload=*/false));

    // One upload was sent.
    histogram_tester.ExpectBucketCount("Autofill.UploadEvent", 1, 1);
    histogram_tester.ExpectBucketCount(
        AutofillMetrics::SubmissionSourceToUploadEventMetric(submission_source),
        1, 1);

    ASSERT_THAT(payloads(), SizeIs(1));
    AutofillUploadRequest request;
    ASSERT_TRUE(request.ParseFromString(payloads().front()));
    ASSERT_TRUE(request.has_upload());
    const AutofillUploadContents& upload = request.upload();
    EXPECT_EQ(upload.language(),
              form_structure.current_page_language().value());
    // Only first upload has metadata.
    const bool expect_metadata = i == 0;
    EXPECT_EQ(upload.has_randomized_form_metadata(), expect_metadata);
    ASSERT_EQ(
        std::all_of(upload.field_data().begin(), upload.field_data().end(),
                    [](AutofillUploadContents::Field field) {
                      return field.has_randomized_field_metadata();
                    }),
        expect_metadata);
    if (expect_metadata) {
      EXPECT_TRUE(upload.randomized_form_metadata().has_id());
      EXPECT_TRUE(upload.randomized_form_metadata().has_name());
      EXPECT_TRUE(upload.randomized_form_metadata().has_url());
      ASSERT_TRUE(upload.randomized_form_metadata().url().has_checksum());
      EXPECT_EQ(upload.randomized_form_metadata().url().checksum(), 3608731642);
      EXPECT_EQ(upload.field_data_size(), 3);
      for (const auto& f : upload.field_data()) {
        EXPECT_TRUE(f.randomized_field_metadata().has_id());
        EXPECT_TRUE(f.randomized_field_metadata().has_name());
        EXPECT_TRUE(f.randomized_field_metadata().has_type());
        EXPECT_TRUE(f.randomized_field_metadata().has_label());
        EXPECT_TRUE(f.randomized_field_metadata().has_aria_label());
        EXPECT_TRUE(f.randomized_field_metadata().has_aria_description());
        EXPECT_TRUE(f.randomized_field_metadata().has_css_class());
        EXPECT_TRUE(f.randomized_field_metadata().has_placeholder());
      }
    }
  }
}

TEST_P(AutofillUploadTest, Throttling) {
  ASSERT_NE(DISABLED, GetParam());

  AutofillCrowdsourcingManager crowdsourcing_manager(
      &client(), version_info::Channel::UNKNOWN, nullptr);
  FormStructure form_structure(
      test::GetFormData({.fields = {{.role = NAME_FIRST},
                                    {.role = NAME_LAST},
                                    {.role = EMAIL_ADDRESS}}}));
  for (int i = 0; i <= static_cast<int>(SubmissionSource::kMaxValue); ++i) {
    base::HistogramTester histogram_tester;
    auto submission_source = static_cast<SubmissionSource>(i);
    SCOPED_TRACE(testing::Message()
                 << "submission source = " << submission_source);
    form_structure.set_submission_source(submission_source);

    // The first attempt should succeed.
    EXPECT_TRUE(SendUploadRequest(form_structure, /*available_field_types=*/{},
                                  /*login_form_signature=*/"",
                                  /*observed_submission=*/true,
                                  /*is_password_manager_upload=*/false));

    // The second attempt should always fail.
    EXPECT_FALSE(SendUploadRequest(form_structure, /*available_field_types=*/{},
                                   /*login_form_signature=*/"",
                                   /*observed_submission=*/true,
                                   /*is_password_manager_upload=*/false));

    // One upload was not sent.
    histogram_tester.ExpectBucketCount("Autofill.UploadEvent", 0, 1);
    histogram_tester.ExpectBucketCount(
        AutofillMetrics::SubmissionSourceToUploadEventMetric(submission_source),
        0, 1);

    // One upload was sent.
    histogram_tester.ExpectBucketCount("Autofill.UploadEvent", 1, 1);
    histogram_tester.ExpectBucketCount(
        AutofillMetrics::SubmissionSourceToUploadEventMetric(submission_source),
        1, 1);
  }
}

// Tests that votes are not throttled with
// `features::test::kAutofillUploadThrottling` disabled, but metadata is
// throttled regardless of the feature state.
TEST_P(AutofillUploadTest, SuccessfulSubmissionOnDisabledThrottling) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(/*enabled_features=*/{}, /*disabled_features=*/{
                                    features::test::kAutofillUploadThrottling});

  FormStructure form_structure(
      test::GetFormData({.fields = {{.role = NAME_FIRST},
                                    {.role = NAME_LAST},
                                    {.role = EMAIL_ADDRESS}}}));
  SetCorrectFieldHostFormSignatures(form_structure);

  SubmissionSource submission_source = SubmissionSource::FORM_SUBMISSION;
  form_structure.set_submission_source(submission_source);
  form_structure.set_randomized_encoder(
      RandomizedEncoder::Create(client().GetPrefs()));

  base::HistogramTester histogram_tester;
  // The first upload must be successfully sent to the Autofill server.
  EXPECT_TRUE(SendUploadRequest(form_structure, /*available_field_types=*/{},
                                /*login_form_signature=*/"",
                                /*observed_submission=*/true,
                                /*is_password_manager_upload=*/false));

  // The second upload also must be successfully sent to the Autofill server.
  EXPECT_TRUE(SendUploadRequest(form_structure, /*available_field_types=*/{},
                                /*login_form_signature=*/"",
                                /*observed_submission=*/true,
                                /*is_password_manager_upload=*/false));
  histogram_tester.ExpectBucketCount("Autofill.UploadEvent", 1, 2);
  // Two votes are sent to the server.
  histogram_tester.ExpectBucketCount(
      AutofillMetrics::SubmissionSourceToUploadEventMetric(submission_source),
      1, 2);
  // There are no votes that are not sent to the server.
  histogram_tester.ExpectBucketCount(
      AutofillMetrics::SubmissionSourceToUploadEventMetric(submission_source),
      0, 0);

  ASSERT_THAT(payloads(), SizeIs(2));

  AutofillUploadRequest request;
  // The first upload must be successfully sent to the Autofill server.
  ASSERT_TRUE(request.ParseFromString(payloads()[0]));
  ASSERT_TRUE(request.has_upload());
  ASSERT_EQ(request.upload().field_data_size(), 3);
  // Metadata must be stored in non-throttled upload. Since it is encoded, only
  // check for presence.
  EXPECT_TRUE(
      request.upload().field_data(0).randomized_field_metadata().has_label());
  EXPECT_TRUE(
      request.upload().field_data(1).randomized_field_metadata().has_label());
  EXPECT_TRUE(
      request.upload().field_data(2).randomized_field_metadata().has_label());

  // The second upload also must be successfully sent to the Autofill server.
  ASSERT_TRUE(request.ParseFromString(payloads()[1]));
  ASSERT_TRUE(request.has_upload());
  ASSERT_EQ(request.upload().field_data_size(), 3);
  // Metadata must be cleared in throttled uploads.
  EXPECT_FALSE(
      request.upload().field_data(0).randomized_field_metadata().has_label());
  EXPECT_FALSE(
      request.upload().field_data(1).randomized_field_metadata().has_label());
  EXPECT_FALSE(
      request.upload().field_data(2).randomized_field_metadata().has_label());
}

TEST_P(AutofillUploadTest, PeriodicReset) {
  ASSERT_NE(DISABLED, GetParam());

  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndEnableFeatureWithParameters(
      features::test::kAutofillUploadThrottling,
      {{switches::kAutofillUploadThrottlingPeriodInDays, "16"}});

  AutofillCrowdsourcingManager crowdsourcing_manager(
      &client(), version_info::Channel::UNKNOWN, nullptr);
  SubmissionSource submission_source = SubmissionSource::FORM_SUBMISSION;

  FormStructure form_structure(
      test::GetFormData({.fields = {{.role = NAME_FIRST},
                                    {.role = NAME_LAST},
                                    {.role = EMAIL_ADDRESS}}}));
  form_structure.set_submission_source(submission_source);

  base::HistogramTester histogram_tester;

  TestAutofillClock test_clock;
  test_clock.SetNow(AutofillClock::Now());

  // The first attempt should succeed.
  EXPECT_TRUE(SendUploadRequest(form_structure, /*available_field_types=*/{},
                                /*login_form_signature=*/"",
                                /*observed_submission=*/true,
                                /*is_password_manager_upload=*/false));

  // Advance the clock, but not past the reset period. The pref won't reset,
  // so the upload should not be sent.
  test_clock.Advance(base::Days(15));
  EXPECT_FALSE(SendUploadRequest(form_structure, /*available_field_types=*/{},
                                 /*login_form_signature=*/"",
                                 /*observed_submission=*/true,
                                 /*is_password_manager_upload=*/false));

  // Advance the clock beyond the reset period. The pref should be reset and
  // the upload should succeed.
  test_clock.Advance(base::Days(2));  // Total = 29
  EXPECT_TRUE(SendUploadRequest(form_structure, /*available_field_types=*/{},
                                /*login_form_signature=*/"",
                                /*observed_submission=*/true,
                                /*is_password_manager_upload=*/false));

  // One upload was not sent.
  histogram_tester.ExpectBucketCount("Autofill.UploadEvent", 0, 1);
  histogram_tester.ExpectBucketCount(
      AutofillMetrics::SubmissionSourceToUploadEventMetric(submission_source),
      0, 1);

  // Two uploads were sent.
  histogram_tester.ExpectBucketCount("Autofill.UploadEvent", 1, 2);
  histogram_tester.ExpectBucketCount(
      AutofillMetrics::SubmissionSourceToUploadEventMetric(submission_source),
      1, 2);
}

TEST_P(AutofillUploadTest, ResetOnClearUploadHistory) {
  ASSERT_NE(DISABLED, GetParam());

  AutofillCrowdsourcingManager crowdsourcing_manager(
      &client(), version_info::Channel::UNKNOWN, nullptr);
  SubmissionSource submission_source = SubmissionSource::FORM_SUBMISSION;

  FormStructure form_structure(
      test::GetFormData({.fields = {{.role = NAME_FIRST},
                                    {.role = NAME_LAST},
                                    {.role = EMAIL_ADDRESS}}}));
  form_structure.set_submission_source(submission_source);

  base::HistogramTester histogram_tester;

  TestAutofillClock test_clock;
  test_clock.SetNow(AutofillClock::Now());

  // The first attempt should succeed.
  EXPECT_TRUE(SendUploadRequest(
      form_structure, /*available_field_types=*/{}, /*login_form_signature=*/"",
      /*observed_submission=*/true, /*is_password_manager_upload=*/false));

  // Clear the upload throttling history.
  AutofillCrowdsourcingManager::ClearUploadHistory(client().GetPrefs());
  EXPECT_TRUE(SendUploadRequest(
      form_structure, /*available_field_types=*/{}, /*login_form_signature=*/"",
      /*observed_submission=*/true, /*is_password_manager_upload=*/false));

  // Two uploads were sent.
  histogram_tester.ExpectBucketCount("Autofill.UploadEvent", 1, 2);
  histogram_tester.ExpectBucketCount(
      AutofillMetrics::SubmissionSourceToUploadEventMetric(submission_source),
      1, 2);
}

// Tests that password manager uploads will have metadata part of the upload
// throttled, but the vote part of the upload will be sent to the server.
TEST_P(AutofillUploadTest, ThrottleMetadataOnPasswordManagerUploads) {
  FormStructure form_structure(
      test::GetFormData({.fields = {{.role = USERNAME}, {.role = PASSWORD}}}));
  SetCorrectFieldHostFormSignatures(form_structure);

  SubmissionSource submission_source = SubmissionSource::FORM_SUBMISSION;
  form_structure.set_submission_source(submission_source);
  form_structure.set_randomized_encoder(
      RandomizedEncoder::Create(client().GetPrefs()));

  base::HistogramTester histogram_tester;
  // The first upload must be successfully sent to the Autofill server.
  EXPECT_TRUE(SendUploadRequest(form_structure, /*available_field_types=*/{},
                                /*login_form_signature=*/"",
                                /*observed_submission=*/true,
                                /*is_password_manager_upload=*/true));

  // The second upload also must be successfully sent to the Autofill server.
  EXPECT_TRUE(SendUploadRequest(form_structure, /*available_field_types=*/{},
                                /*login_form_signature=*/"",
                                /*observed_submission=*/true,
                                /*is_password_manager_upload=*/true));
  histogram_tester.ExpectBucketCount("Autofill.UploadEvent", 1, 2);
  // Two votes are sent to the server.
  histogram_tester.ExpectBucketCount(
      AutofillMetrics::SubmissionSourceToUploadEventMetric(submission_source),
      1, 2);
  // There are no votes that are not sent to the server.
  histogram_tester.ExpectBucketCount(
      AutofillMetrics::SubmissionSourceToUploadEventMetric(submission_source),
      0, 0);

  ASSERT_THAT(payloads(), SizeIs(2));

  AutofillUploadRequest request;
  // The first upload request received by the server must be parseable and
  // contain 2 fields.
  ASSERT_TRUE(request.ParseFromString(payloads()[0]));
  ASSERT_TRUE(request.has_upload());
  ASSERT_EQ(request.upload().field_data_size(), 2);
  // Metadata must be stored in non-throttled upload. Since the metadata is
  // encoded, only check for presence.
  EXPECT_TRUE(
      request.upload().field_data(0).randomized_field_metadata().has_label());
  EXPECT_TRUE(
      request.upload().field_data(1).randomized_field_metadata().has_label());

  // The second upload request received by the server must be parseable and
  // contain 2 fields.
  ASSERT_TRUE(request.ParseFromString(payloads()[1]));
  ASSERT_TRUE(request.has_upload());
  ASSERT_EQ(request.upload().field_data_size(), 2);
  // Metadata must be cleared in throttled uploads.
  EXPECT_FALSE(
      request.upload().field_data(0).randomized_field_metadata().has_label());
  EXPECT_FALSE(
      request.upload().field_data(1).randomized_field_metadata().has_label());
}

// Note that we omit DEFAULT_URL from the test params. We don't actually want
// the tests to hit the production server. We also excluded DISABLED, since
// these tests exercise "enabled" functionality.
INSTANTIATE_TEST_SUITE_P(All,
                         AutofillUploadTest,
                         ::testing::Values(FINCHED_URL, COMMAND_LINE_URL));

}  // namespace
}  // namespace autofill
