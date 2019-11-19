// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_download_manager.h"

#include <stddef.h>

#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/format_macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_http_header_provider.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/third_party/mozilla/url_parse.h"

using base::UTF8ToUTF16;
using net::test_server::BasicHttpResponse;
using net::test_server::EmbeddedTestServer;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
namespace autofill {

using mojom::SubmissionSource;

namespace {

const int METHOD_GET = 0;
const int METHOD_POST = 1;
const int CACHE_MISS = 0;
const int CACHE_HIT = 1;

std::vector<FormStructure*> ToRawPointerVector(
    const std::vector<std::unique_ptr<FormStructure>>& list) {
  std::vector<FormStructure*> result;
  for (const auto& item : list)
    result.push_back(item.get());
  return result;
}

// Puts all data elements within the response body together in a single
// DataElement and return the buffered content as a string. This ensure all
// the response body data is utilized.
std::string GetStringFromDataElements(
    const std::vector<network::DataElement>* data_elements) {
  network::DataElement unified_data_element;
  auto data_elements_it = data_elements->begin();
  if (data_elements_it != data_elements->end()) {
    unified_data_element.SetToBytes(data_elements_it->bytes(),
                                    data_elements_it->length());
  }
  ++data_elements_it;
  while (data_elements_it != data_elements->end()) {
    unified_data_element.AppendBytes(data_elements_it->bytes(),
                                     data_elements_it->length());
    ++data_elements_it;
  }
  // Using the std::string constructor with length ensures that we don't rely
  // on having a termination character to delimit the string. This is the
  // safest approach.
  return std::string(unified_data_element.bytes(),
                     unified_data_element.length());
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

bool DeserializeAutofillPageQueryRequest(base::StringPiece serialized_content,
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

class AutofillDownloadManagerWithCustomPayloadSize
    : public AutofillDownloadManager {
 public:
  ~AutofillDownloadManagerWithCustomPayloadSize() override {}
  AutofillDownloadManagerWithCustomPayloadSize(AutofillDriver* driver,
                                               Observer* observer,
                                               const std::string& api_key,
                                               size_t length)
      : AutofillDownloadManager(driver,
                                observer,
                                api_key,
                                /*log_manager=*/nullptr),
        length_(length) {}

 protected:
  size_t GetPayloadLength(base::StringPiece payload) const override {
    return length_;
  }

 private:
  size_t length_;
};

}  // namespace

// This tests AutofillDownloadManager. AutofillDownloadManagerTest implements
// AutofillDownloadManager::Observer and creates an instance of
// AutofillDownloadManager. Then it records responses to different initiated
// requests, which are verified later. To mock network requests
// TestURLLoaderFactory is used, which creates SimpleURLLoaders that do not
// go over the wire, but allow calling back HTTP responses directly.
// The responses in test are out of order and verify: successful query request,
// successful upload request, failed upload request.
class AutofillDownloadManagerTest : public AutofillDownloadManager::Observer,
                                    public ::testing::Test {
 public:
  AutofillDownloadManagerTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        download_manager_(&driver_, this),
        pref_service_(test::PrefServiceForTesting()) {
    driver_.SetSharedURLLoaderFactory(test_shared_loader_factory_);
  }

  void LimitCache(size_t cache_size) {
    download_manager_.set_max_form_cache_size(cache_size);
  }

  // AutofillDownloadManager::Observer implementation.
  void OnLoadedServerPredictions(
      std::string response_xml,
      const std::vector<std::string>& form_signatures) override {
    ResponseData response;
    response.response = std::move(response_xml);
    response.type_of_response = QUERY_SUCCESSFULL;
    responses_.push_back(response);
  }

  void OnUploadedPossibleFieldTypes() override {
    ResponseData response;
    response.type_of_response = UPLOAD_SUCCESSFULL;
    responses_.push_back(response);
  }

  void OnServerRequestError(const std::string& form_signature,
                            AutofillDownloadManager::RequestType request_type,
                            int http_error) override {
    ResponseData response;
    response.signature = form_signature;
    response.error = http_error;
    response.type_of_response =
        request_type == AutofillDownloadManager::REQUEST_QUERY
            ? REQUEST_QUERY_FAILED
            : REQUEST_UPLOAD_FAILED;
    responses_.push_back(response);
  }

  enum ResponseType {
    QUERY_SUCCESSFULL,
    UPLOAD_SUCCESSFULL,
    REQUEST_QUERY_FAILED,
    REQUEST_UPLOAD_FAILED,
  };

  struct ResponseData {
    ResponseType type_of_response;
    int error;
    std::string signature;
    std::string response;

    ResponseData() : type_of_response(REQUEST_QUERY_FAILED), error(0) {}
  };

  ScopedActiveAutofillExperiments scoped_active_autofill_experiments;
  base::test::TaskEnvironment task_environment_;
  std::list<ResponseData> responses_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestAutofillDriver driver_;
  AutofillDownloadManager download_manager_;
  std::unique_ptr<PrefService> pref_service_;
};

TEST_F(AutofillDownloadManagerTest, QueryAndUploadTest) {
  base::test::ScopedFeatureList fl;
  fl.InitAndEnableFeature(features::kAutofillCacheQueryResponses);

  FormData form;

  FormFieldData field;
  field.label = UTF8ToUTF16("username");
  field.name = UTF8ToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("First Name");
  field.name = UTF8ToUTF16("firstname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("Last Name");
  field.name = UTF8ToUTF16("lastname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("email");
  field.name = UTF8ToUTF16("email");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("email2");
  field.name = UTF8ToUTF16("email2");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("password");
  field.name = UTF8ToUTF16("password");
  field.form_control_type = "password";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = UTF8ToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  std::vector<std::unique_ptr<FormStructure>> form_structures;
  form_structures.push_back(std::make_unique<FormStructure>(form));

  form.fields.clear();

  field.label = UTF8ToUTF16("address");
  field.name = UTF8ToUTF16("address");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("address2");
  field.name = UTF8ToUTF16("address2");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("city");
  field.name = UTF8ToUTF16("city");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = UTF8ToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structures.push_back(std::make_unique<FormStructure>(form));

  form.fields.clear();

  field.label = UTF8ToUTF16("username");
  field.name = UTF8ToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("password");
  field.name = UTF8ToUTF16("password");
  field.form_control_type = "password";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = UTF8ToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structures.push_back(std::make_unique<FormStructure>(form));

  // Make download manager.
  AutofillDownloadManager download_manager(&driver_, this, "dummykey",
                                           /*log_manager=*/nullptr);

  // Request with id 0.
  base::HistogramTester histogram;
  EXPECT_TRUE(
      download_manager.StartQueryRequest(ToRawPointerVector(form_structures)));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 1);
  histogram.ExpectUniqueSample("Autofill.Query.Method", METHOD_GET, 1);

  // Validate if the API key is in the request headers.
  network::TestURLLoaderFactory::PendingRequest* request =
      test_url_loader_factory_.GetPendingRequest(0);
  std::string api_key_header_value;
  EXPECT_TRUE(request->request.headers.GetHeader("X-Goog-Api-Key",
                                                 &api_key_header_value));
  EXPECT_EQ(api_key_header_value, "dummykey");

  // Request with id 1.
  EXPECT_TRUE(download_manager.StartUploadRequest(
      *(form_structures[0]), true, ServerFieldTypeSet(), std::string(), true,
      pref_service_.get()));
  // Request with id 2.
  EXPECT_TRUE(download_manager.StartUploadRequest(
      *(form_structures[1]), false, ServerFieldTypeSet(), std::string(), true,
      pref_service_.get()));
  // Request with id 3. Upload request with a non-empty additional password form
  // signature.
  EXPECT_TRUE(download_manager.StartUploadRequest(*(form_structures[2]), false,
                                                  ServerFieldTypeSet(), "42",
                                                  true, pref_service_.get()));

  const char* responses[] = {
      "<autofillqueryresponse>"
      "<field autofilltype=\"0\" />"
      "<field autofilltype=\"3\" />"
      "<field autofilltype=\"5\" />"
      "<field autofilltype=\"9\" />"
      "<field autofilltype=\"0\" />"
      "<field autofilltype=\"30\" />"
      "<field autofilltype=\"31\" />"
      "<field autofilltype=\"33\" />"
      "</autofillqueryresponse>",
      "",
      "<html></html>",
  };

  // Return them out of sequence.

  // Request 1: Successful upload.
  request = test_url_loader_factory_.GetPendingRequest(1);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, responses[1]);
  histogram.ExpectBucketCount("Autofill.Upload.HttpResponseOrErrorCode",
                              net::HTTP_OK, 1);

  // Request 2: Unsuccessful upload.
  request = test_url_loader_factory_.GetPendingRequest(2);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, network::CreateURLResponseHead(net::HTTP_NOT_FOUND),
      responses[2], network::URLLoaderCompletionStatus(net::OK));
  histogram.ExpectBucketCount("Autofill.Upload.HttpResponseOrErrorCode",
                              net::HTTP_NOT_FOUND, 1);

  // Request 0: Successful query.
  request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, responses[0]);
  EXPECT_EQ(3U, responses_.size());
  histogram.ExpectBucketCount("Autofill.Query.WasInCache", CACHE_MISS, 1);
  histogram.ExpectBucketCount("Autofill.Query.HttpResponseOrErrorCode",
                              net::HTTP_OK, 1);

  // Check Request 1.
  EXPECT_EQ(AutofillDownloadManagerTest::UPLOAD_SUCCESSFULL,
            responses_.front().type_of_response);
  EXPECT_EQ(0, responses_.front().error);
  EXPECT_EQ(std::string(), responses_.front().signature);
  // Expected response on non-query request is an empty string.
  EXPECT_EQ(std::string(), responses_.front().response);
  responses_.pop_front();

  // Check Request 2.
  EXPECT_EQ(AutofillDownloadManagerTest::REQUEST_UPLOAD_FAILED,
            responses_.front().type_of_response);
  EXPECT_EQ(net::HTTP_NOT_FOUND, responses_.front().error);
  EXPECT_EQ(form_structures[1]->FormSignatureAsStr(),
            responses_.front().signature);
  // Expected response on non-query request is an empty string.
  EXPECT_EQ(std::string(), responses_.front().response);
  responses_.pop_front();

  // Check Request 0.
  EXPECT_EQ(responses_.front().type_of_response,
            AutofillDownloadManagerTest::QUERY_SUCCESSFULL);
  EXPECT_EQ(0, responses_.front().error);
  EXPECT_EQ(std::string(), responses_.front().signature);
  EXPECT_EQ(responses[0], responses_.front().response);
  responses_.pop_front();

  // Modify form structures to miss the cache.
  field.label = UTF8ToUTF16("Address line 2");
  field.name = UTF8ToUTF16("address2");
  field.form_control_type = "text";
  form.fields.push_back(field);
  form_structures.push_back(std::make_unique<FormStructure>(form));

  // Request with id 4, not successful.
  EXPECT_TRUE(
      download_manager.StartQueryRequest(ToRawPointerVector(form_structures)));
  request = test_url_loader_factory_.GetPendingRequest(4);
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 2);
  histogram.ExpectUniqueSample("Autofill.Query.Method", METHOD_GET, 2);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR),
      responses[0], network::URLLoaderCompletionStatus(net::OK));
  histogram.ExpectBucketCount("Autofill.Query.HttpResponseOrErrorCode",
                              net::HTTP_INTERNAL_SERVER_ERROR, 1);

  // Check Request 4.
  EXPECT_EQ(AutofillDownloadManagerTest::REQUEST_QUERY_FAILED,
            responses_.front().type_of_response);
  EXPECT_EQ(net::HTTP_INTERNAL_SERVER_ERROR, responses_.front().error);
  // Expected response on non-query request is an empty string.
  EXPECT_EQ(std::string(), responses_.front().response);
  responses_.pop_front();

  // Request with id 5. Let's pretend we hit the cache.
  EXPECT_TRUE(
      download_manager.StartQueryRequest(ToRawPointerVector(form_structures)));
  histogram.ExpectBucketCount("Autofill.ServerQueryResponse",
                              AutofillMetrics::QUERY_SENT, 3);
  histogram.ExpectBucketCount("Autofill.Query.Method", METHOD_GET, 3);
  request = test_url_loader_factory_.GetPendingRequest(5);

  network::URLLoaderCompletionStatus status(net::OK);
  status.exists_in_cache = true;
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, network::CreateURLResponseHead(net::HTTP_OK), responses[0],
      status);

  // Check Request 5.
  EXPECT_EQ(responses_.front().type_of_response,
            AutofillDownloadManagerTest::QUERY_SUCCESSFULL);
  responses_.pop_front();
  histogram.ExpectBucketCount("Autofill.Query.WasInCache", CACHE_HIT, 1);

  // Test query with caching disabled.
  base::test::ScopedFeatureList fl2;
  fl2.InitAndDisableFeature(features::kAutofillCacheQueryResponses);

  // Don't hit the in-mem cache.
  field.label = UTF8ToUTF16("Address line 3");
  field.name = UTF8ToUTF16("address3");
  field.form_control_type = "text";
  form.fields.push_back(field);
  form_structures.push_back(std::make_unique<FormStructure>(form));

  // Request with id 6
  EXPECT_TRUE(
      download_manager.StartQueryRequest(ToRawPointerVector(form_structures)));
  histogram.ExpectBucketCount("Autofill.ServerQueryResponse",
                              AutofillMetrics::QUERY_SENT, 4);
  histogram.ExpectBucketCount("Autofill.Query.Method", METHOD_POST, 1);
  request = test_url_loader_factory_.GetPendingRequest(6);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, responses[0]);
  histogram.ExpectBucketCount("Autofill.Query.WasInCache", CACHE_MISS, 2);
}

TEST_F(AutofillDownloadManagerTest, QueryAPITest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // Enabled
      // We want to query the API rather than the legacy server.
      {features::kAutofillUseApi},
      // Disabled
      {});

  // Build the form structures that we want to query.
  FormData form;
  FormFieldData field;

  field.label = UTF8ToUTF16("First Name");
  field.name = UTF8ToUTF16("firstname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("Last Name");
  field.name = UTF8ToUTF16("lastname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  std::vector<std::unique_ptr<FormStructure>> form_structures;
  form_structures.push_back(std::make_unique<FormStructure>(form));

  AutofillDownloadManager download_manager(&driver_, this, "dummykey",
                                           /*log_manager=*/nullptr);

  // Start the query request and look if it is successful. No response was
  // received yet.
  base::HistogramTester histogram;
  EXPECT_TRUE(
      download_manager.StartQueryRequest(ToRawPointerVector(form_structures)));

  // Verify if histograms are right.
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 1);
  histogram.ExpectUniqueSample("Autofill.Query.Method", METHOD_GET, 1);
  {
    auto buckets = histogram.GetAllSamples("Autofill.Query.GetUrlLength");
    ASSERT_EQ(1U, buckets.size());
    EXPECT_GT(buckets[0].count, 0);
  }
  histogram.ExpectUniqueSample("Autofill.Query.ApiUrlIsTooLong", false, 1);

  // Inspect the request that the test URL loader sent.
  network::TestURLLoaderFactory::PendingRequest* request =
      test_url_loader_factory_.GetPendingRequest(0);

  // Verify request URL and the data payload it carries.
  {
    // This is the URL we expect to query the API. The sub-path right after
    // "/page" corresponds to the serialized AutofillPageQueryRequest proto
    // (that we filled forms in) encoded in base64. The Autofill
    // https://clients1.google.com/ domain URL corresponds to the default domain
    // used by the download manager, which is invalid, but good for testing.
    const std::string expected_url =
        R"(https://clients1.google.com/v1/pages/(.+)\?alt=proto)";
    std::string encoded_request;
    ASSERT_TRUE(re2::RE2::FullMatch(request->request.url.spec(), expected_url,
                                    &encoded_request));
    AutofillPageQueryRequest request_content;
    ASSERT_TRUE(
        DeserializeAutofillPageQueryRequest(encoded_request, &request_content));
    // Verify form content.
    ASSERT_EQ(request_content.forms().size(), 1);
    EXPECT_EQ(request_content.forms(0).signature(),
              form_structures[0]->form_signature());
    // Verify field content.
    ASSERT_EQ(request_content.forms(0).fields().size(), 2);
    EXPECT_EQ(request_content.forms(0).fields(0).signature(),
              form_structures[0]->field(0)->GetFieldSignature());
    EXPECT_EQ(request_content.forms(0).fields(1).signature(),
              form_structures[0]->field(1)->GetFieldSignature());
  }

  // Verify API key header.
  {
    std::string header_value;
    EXPECT_TRUE(
        request->request.headers.GetHeader("X-Goog-Api-Key", &header_value));
    EXPECT_EQ(header_value, "dummykey");
  }
  // Verify binary response header.
  {
    std::string header_value;
    ASSERT_TRUE(request->request.headers.GetHeader(
        "X-Goog-Encode-Response-If-Executable", &header_value));
    EXPECT_EQ(header_value, "base64");
  }

  // Verify response.
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, "dummy response");
  // Upon reception of a suggestions query, we expect OnLoadedServerPredictions
  // to be called back from the observer and some histograms be incremented.
  EXPECT_EQ(1U, responses_.size());
  EXPECT_EQ(responses_.front().type_of_response,
            AutofillDownloadManagerTest::QUERY_SUCCESSFULL);
  histogram.ExpectBucketCount("Autofill.Query.WasInCache", CACHE_MISS, 1);
  histogram.ExpectBucketCount("Autofill.Query.HttpResponseOrErrorCode",
                              net::HTTP_OK, 1);
}

TEST_F(AutofillDownloadManagerTest, QueryAPITestWhenTooLongUrl) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // Enabled
      // We want to query the API rather than the legacy server.
      {features::kAutofillUseApi},
      // Disabled
      {});

  // Build the form structures that we want to query.
  FormData form;
  FormFieldData field;
  field.label = UTF8ToUTF16("First Name");
  field.name = UTF8ToUTF16("firstname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  std::vector<std::unique_ptr<FormStructure>> form_structures;
  {
    auto form_structure = std::make_unique<FormStructure>(form);
    form_structure->set_is_rich_query_enabled(true);
    form_structures.push_back(std::move(form_structure));
  }

  AutofillDownloadManagerWithCustomPayloadSize download_manager(
      &driver_, this, "dummykey", kMaxAPIQueryGetSize + 1);

  // Start the query request and look if it is successful. No response was
  // received yet.
  base::HistogramTester histogram;
  EXPECT_TRUE(
      download_manager.StartQueryRequest(ToRawPointerVector(form_structures)));

  // Verify request.
  // Verify if histograms are right.
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 1);
  // Verify that the logged method is POST.
  histogram.ExpectUniqueSample("Autofill.Query.Method", METHOD_POST, 1);
  // Verify that too long URL is tracked.
  histogram.ExpectUniqueSample("Autofill.Query.ApiUrlIsTooLong", true, 1);

  // Get the latest request that the test URL loader sent.
  network::TestURLLoaderFactory::PendingRequest* request =
      test_url_loader_factory_.GetPendingRequest(0);
  // Verify that the POST URL is used when request data too large.
  const std::string expected_url = {
      "https://clients1.google.com/v1/pages:get?alt=proto"};
  // Verify API key header.
  EXPECT_EQ(request->request.url, expected_url);
  {
    std::string header_value;
    EXPECT_TRUE(
        request->request.headers.GetHeader("X-Goog-Api-Key", &header_value));
    EXPECT_EQ(header_value, "dummykey");
  }
  // Verify Content-Type header.
  {
    std::string header_value;
    ASSERT_TRUE(
        request->request.headers.GetHeader("Content-Type", &header_value));
    EXPECT_EQ(header_value, "application/x-protobuf");
  }
  // Verify binary response header.
  {
    std::string header_value;
    ASSERT_TRUE(request->request.headers.GetHeader(
        "X-Goog-Encode-Response-If-Executable", &header_value));
    EXPECT_EQ(header_value, "base64");
  }
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
    EXPECT_EQ(request_content.forms(0).signature(),
              form_structures[0]->form_signature());
    // Verify field content.
    ASSERT_EQ(request_content.forms(0).fields().size(), 1);
    EXPECT_EQ(request_content.forms(0).fields(0).signature(),
              form_structures[0]->field(0)->GetFieldSignature());
  }

  // Verify response.
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, "dummy response");
  // Upon reception of a suggestions query, we expect OnLoadedServerPredictions
  // to be called back from the observer and some histograms be incremented.
  EXPECT_EQ(1U, responses_.size());
  EXPECT_EQ(responses_.front().type_of_response,
            AutofillDownloadManagerTest::QUERY_SUCCESSFULL);
  histogram.ExpectBucketCount("Autofill.Query.WasInCache", CACHE_MISS, 1);
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
TEST_F(AutofillDownloadManagerTest, UploadToAPITest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // Enabled
      // We want to query the API rather than the legacy server.
      {features::kAutofillUseApi},
      // Disabled
      // We don't want upload throttling for testing purpose.
      {features::kAutofillUploadThrottling});

  // Build the form structures that we want to query.
  FormData form;
  FormFieldData field;

  field.label = UTF8ToUTF16("First Name");
  field.name = UTF8ToUTF16("firstname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("Last Name");
  field.name = UTF8ToUTF16("lastname");
  field.form_control_type = "text";
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.set_submission_source(SubmissionSource::FORM_SUBMISSION);

  std::unique_ptr<PrefService> pref_service = test::PrefServiceForTesting();
  AutofillDownloadManager download_manager(&driver_, this, "dummykey",
                                           /*log_manager=*/nullptr);
  EXPECT_TRUE(download_manager.StartUploadRequest(form_structure, true,
                                                  ServerFieldTypeSet(), "",
                                                  true, pref_service.get()));

  // Inspect the request that the test URL loader sent.
  network::TestURLLoaderFactory::PendingRequest* request =
      test_url_loader_factory_.GetPendingRequest(0);

  // This is the URL we expect to upload votes to the API. The Autofill
  // https://clients1.google.com/ domain URL corresponds to the
  // default one used by the download manager. Request upload data is in the
  // payload when uploading.
  const std::string expected_url =
      "https://clients1.google.com/v1/forms:vote?alt=proto";
  EXPECT_EQ(request->request.url, expected_url);
  std::string api_key_header_value;
  EXPECT_TRUE(request->request.headers.GetHeader("X-Goog-Api-Key",
                                                 &api_key_header_value));
  EXPECT_EQ(api_key_header_value, "dummykey");

  // Assert some of the fields within the uploaded proto to make sure it was
  // filled with something else than default data.
  base::HistogramTester histogram;
  AutofillUploadRequest upload_request;
  EXPECT_TRUE(GetUploadRequestProtoFromRequest(request, &upload_request));
  EXPECT_GT(upload_request.upload().client_version().size(), 0U);
  EXPECT_EQ(upload_request.upload().form_signature(),
            form_structure.form_signature());

  // Trigger an upload response from the API and assert upload response content.
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, "");
  // Upon reception of a suggestions query, we expect
  // OnUploadedPossibleFieldTypes  to be called back from the observer and some
  // histograms be incremented.
  EXPECT_EQ(1U, responses_.size());
  // Request should be upload and successful.
  EXPECT_EQ(AutofillDownloadManagerTest::UPLOAD_SUCCESSFULL,
            responses_.front().type_of_response);
  // We expect the request to be OK and corresponding response code to be
  // counted.
  histogram.ExpectBucketCount("Autofill.Upload.HttpResponseOrErrorCode",
                              net::HTTP_OK, 1);
}

TEST_F(AutofillDownloadManagerTest, BackoffLogic_Query) {
  FormData form;
  FormFieldData field;
  field.label = UTF8ToUTF16("address");
  field.name = UTF8ToUTF16("address");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("address2");
  field.name = UTF8ToUTF16("address2");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("city");
  field.name = UTF8ToUTF16("city");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = UTF8ToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  std::vector<std::unique_ptr<FormStructure>> form_structures;
  form_structures.push_back(std::make_unique<FormStructure>(form));

  // Request with id 0.
  base::HistogramTester histogram;
  EXPECT_TRUE(
      download_manager_.StartQueryRequest(ToRawPointerVector(form_structures)));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 1);

  auto* request = test_url_loader_factory_.GetPendingRequest(0);

  // Request error incurs a retry after 1 second.
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR),
      "", network::URLLoaderCompletionStatus(net::OK));

  EXPECT_EQ(1U, responses_.size());
  EXPECT_LT(download_manager_.loader_backoff_.GetTimeUntilRelease(),
            base::TimeDelta::FromMilliseconds(1100));
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(1100));
  run_loop.Run();

  // Get the retried request.
  request = test_url_loader_factory_.GetPendingRequest(1);

  // Next error incurs a retry after 2 seconds.
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request,
      network::CreateURLResponseHead(net::HTTP_REQUEST_ENTITY_TOO_LARGE),
      "<html></html>", network::URLLoaderCompletionStatus(net::OK));

  EXPECT_EQ(2U, responses_.size());
  EXPECT_LT(download_manager_.loader_backoff_.GetTimeUntilRelease(),
            base::TimeDelta::FromMilliseconds(2100));

  // There should not be an additional retry.
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 0);
  histogram.ExpectBucketCount("Autofill.Query.HttpResponseOrErrorCode",
                              net::HTTP_REQUEST_ENTITY_TOO_LARGE, 1);
  auto buckets = histogram.GetAllSamples("Autofill.Query.FailingPayloadSize");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_EQ(2, buckets[0].count);
}

TEST_F(AutofillDownloadManagerTest, BackoffLogic_Upload) {
  FormData form;
  FormFieldData field;
  field.label = UTF8ToUTF16("address");
  field.name = UTF8ToUTF16("address");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("address2");
  field.name = UTF8ToUTF16("address2");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("city");
  field.name = UTF8ToUTF16("city");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = UTF8ToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_submission_source(SubmissionSource::FORM_SUBMISSION);

  // Request with id 0.
  EXPECT_TRUE(download_manager_.StartUploadRequest(
      *form_structure, true, ServerFieldTypeSet(), std::string(), true,
      pref_service_.get()));

  auto* request = test_url_loader_factory_.GetPendingRequest(0);

  // Error incurs a retry after 1 second.
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR),
      "", network::URLLoaderCompletionStatus(net::OK));
  EXPECT_EQ(1U, responses_.size());
  EXPECT_LT(download_manager_.loader_backoff_.GetTimeUntilRelease(),
            base::TimeDelta::FromMilliseconds(1100));
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(1100));
  run_loop.Run();

  // Check that it was a failure.
  EXPECT_EQ(AutofillDownloadManagerTest::REQUEST_UPLOAD_FAILED,
            responses_.front().type_of_response);
  EXPECT_EQ(net::HTTP_INTERNAL_SERVER_ERROR, responses_.front().error);
  EXPECT_EQ(form_structure->FormSignatureAsStr(), responses_.front().signature);
  // Expected response on non-query request is an empty string.
  EXPECT_EQ(std::string(), responses_.front().response);
  responses_.pop_front();

  // Get the retried request, and make it successful.
  request = test_url_loader_factory_.GetPendingRequest(1);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, "");

  // Check success of response.
  EXPECT_EQ(AutofillDownloadManagerTest::UPLOAD_SUCCESSFULL,
            responses_.front().type_of_response);
  EXPECT_EQ(0, responses_.front().error);
  EXPECT_EQ(std::string(), responses_.front().signature);
  // Expected response on non-query request is an empty string.
  EXPECT_EQ(std::string(), responses_.front().response);
  responses_.pop_front();

  // Validate no retry on sending a bad request.
  form_structure->set_submission_source(SubmissionSource::XHR_SUCCEEDED);
  base::HistogramTester histogram;
  EXPECT_TRUE(download_manager_.StartUploadRequest(
      *form_structure, true, ServerFieldTypeSet(), std::string(), true,
      pref_service_.get()));
  request = test_url_loader_factory_.GetPendingRequest(2);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request,
      network::CreateURLResponseHead(net::HTTP_REQUEST_ENTITY_TOO_LARGE), "",
      network::URLLoaderCompletionStatus(net::OK));
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 0);
  histogram.ExpectBucketCount("Autofill.Upload.HttpResponseOrErrorCode",
                              net::HTTP_REQUEST_ENTITY_TOO_LARGE, 1);
  auto buckets = histogram.GetAllSamples("Autofill.Upload.FailingPayloadSize");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_EQ(1, buckets[0].count);
}

TEST_F(AutofillDownloadManagerTest, RetryLimit_Query) {
  FormData form;
  FormFieldData field;
  field.label = UTF8ToUTF16("address");
  field.name = UTF8ToUTF16("address");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("address2");
  field.name = UTF8ToUTF16("address2");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("city");
  field.name = UTF8ToUTF16("city");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = UTF8ToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  std::vector<std::unique_ptr<FormStructure>> form_structures;
  form_structures.push_back(std::make_unique<FormStructure>(form));

  // Request with id 0.
  base::HistogramTester histogram;
  EXPECT_TRUE(
      download_manager_.StartQueryRequest(ToRawPointerVector(form_structures)));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 1);

  const auto kTimeDeltaMargin = base::TimeDelta::FromMilliseconds(100);
  const int max_attempts = download_manager_.GetMaxServerAttempts();
  int attempt = 0;
  while (true) {
    auto* request = test_url_loader_factory_.GetPendingRequest(attempt);
    ASSERT_TRUE(request != nullptr);

    // Request error incurs a retry after 1 second.
    test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
        request,
        network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR),
        "<html></html>", network::URLLoaderCompletionStatus(net::OK));

    EXPECT_EQ(1U, responses_.size());
    const auto& response = responses_.front();
    EXPECT_EQ(AutofillDownloadManagerTest::REQUEST_QUERY_FAILED,
              response.type_of_response);
    EXPECT_EQ(net::HTTP_INTERNAL_SERVER_ERROR, response.error);
    responses_.pop_front();

    if (++attempt >= max_attempts)
      break;

    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        download_manager_.loader_backoff_.GetTimeUntilRelease() +
            kTimeDeltaMargin);
    run_loop.Run();
  }

  // There should not be an additional retry.
  EXPECT_EQ(attempt, max_attempts);
  EXPECT_EQ(nullptr, test_url_loader_factory_.GetPendingRequest(attempt));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  // Verify metrics.
  histogram.ExpectBucketCount("Autofill.Query.HttpResponseOrErrorCode",
                              net::HTTP_INTERNAL_SERVER_ERROR, max_attempts);
  auto buckets = histogram.GetAllSamples("Autofill.Query.FailingPayloadSize");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_EQ(max_attempts, buckets[0].count);
}

TEST_F(AutofillDownloadManagerTest, RetryLimit_Upload) {
  FormData form;
  FormFieldData field;
  field.label = UTF8ToUTF16("address");
  field.name = UTF8ToUTF16("address");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("address2");
  field.name = UTF8ToUTF16("address2");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("city");
  field.name = UTF8ToUTF16("city");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = UTF8ToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  base::HistogramTester histogram;
  const auto kTimeDeltaMargin = base::TimeDelta::FromMilliseconds(100);

  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_submission_source(SubmissionSource::FORM_SUBMISSION);

  // Request with id 0.
  EXPECT_TRUE(download_manager_.StartUploadRequest(
      *form_structure, true, ServerFieldTypeSet(), std::string(), true,
      pref_service_.get()));

  const int max_attempts = download_manager_.GetMaxServerAttempts();
  int attempt = 0;
  while (true) {
    auto* request = test_url_loader_factory_.GetPendingRequest(attempt);
    ASSERT_TRUE(request != nullptr);

    // Simulate a server failure.
    test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
        request,
        network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR), "",
        network::URLLoaderCompletionStatus(net::OK));

    // Check that it was a failure.
    ASSERT_EQ(1U, responses_.size());
    const auto& response = responses_.front();
    EXPECT_EQ(AutofillDownloadManagerTest::REQUEST_UPLOAD_FAILED,
              response.type_of_response);
    EXPECT_EQ(net::HTTP_INTERNAL_SERVER_ERROR, response.error);
    responses_.pop_front();

    if (++attempt >= max_attempts)
      break;

    // A retry should have been scheduled with wait time on the order of
    // |delay|.
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        download_manager_.loader_backoff_.GetTimeUntilRelease() +
            kTimeDeltaMargin);
    run_loop.Run();
  }

  // There should not be an additional retry.
  EXPECT_EQ(attempt, max_attempts);
  EXPECT_EQ(nullptr, test_url_loader_factory_.GetPendingRequest(attempt));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  // Verify metrics.
  histogram.ExpectBucketCount("Autofill.Upload.HttpResponseOrErrorCode",
                              net::HTTP_INTERNAL_SERVER_ERROR, max_attempts);
  auto buckets = histogram.GetAllSamples("Autofill.Upload.FailingPayloadSize");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_EQ(max_attempts, buckets[0].count);
}

TEST_F(AutofillDownloadManagerTest, QueryTooManyFieldsTest) {
  // Create a query that contains too many fields for the server.
  std::vector<FormData> forms(21);
  std::vector<std::unique_ptr<FormStructure>> form_structures;
  for (auto& form : forms) {
    for (size_t i = 0; i < 5; ++i) {
      FormFieldData field;
      field.label = base::NumberToString16(i);
      field.name = base::NumberToString16(i);
      field.form_control_type = "text";
      form.fields.push_back(field);
    }
    form_structures.push_back(std::make_unique<FormStructure>(form));
  }

  // Check whether the query is aborted.
  EXPECT_FALSE(
      download_manager_.StartQueryRequest(ToRawPointerVector(form_structures)));
}

TEST_F(AutofillDownloadManagerTest, QueryNotTooManyFieldsTest) {
  // Create a query that contains a lot of fields, but not too many for the
  // server.
  std::vector<FormData> forms(25);
  std::vector<std::unique_ptr<FormStructure>> form_structures;
  for (auto& form : forms) {
    for (size_t i = 0; i < 4; ++i) {
      FormFieldData field;
      field.label = base::NumberToString16(i);
      field.name = base::NumberToString16(i);
      field.form_control_type = "text";
      form.fields.push_back(field);
    }
    form_structures.push_back(std::make_unique<FormStructure>(form));
  }

  // Check that the query is not aborted.
  EXPECT_TRUE(
      download_manager_.StartQueryRequest(ToRawPointerVector(form_structures)));
}

TEST_F(AutofillDownloadManagerTest, CacheQueryTest) {
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = UTF8ToUTF16("username");
  field.name = UTF8ToUTF16("username");
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("First Name");
  field.name = UTF8ToUTF16("firstname");
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("Last Name");
  field.name = UTF8ToUTF16("lastname");
  form.fields.push_back(field);

  std::vector<std::unique_ptr<FormStructure>> form_structures0;
  form_structures0.push_back(std::make_unique<FormStructure>(form));

  // Add a slightly different form, which should result in a different request.
  field.label = UTF8ToUTF16("email");
  field.name = UTF8ToUTF16("email");
  form.fields.push_back(field);
  std::vector<std::unique_ptr<FormStructure>> form_structures1;
  form_structures1.push_back(std::make_unique<FormStructure>(form));

  // Add another slightly different form, which should also result in a
  // different request.
  field.label = UTF8ToUTF16("email2");
  field.name = UTF8ToUTF16("email2");
  form.fields.push_back(field);
  std::vector<std::unique_ptr<FormStructure>> form_structures2;
  form_structures2.push_back(std::make_unique<FormStructure>(form));

  // Limit cache to two forms.
  LimitCache(2);

  const char* responses[] = {
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
  EXPECT_TRUE(download_manager_.StartQueryRequest(
      ToRawPointerVector(form_structures0)));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 1);

  // No responses yet
  EXPECT_EQ(0U, responses_.size());

  auto* request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, responses[0]);
  ASSERT_EQ(1U, responses_.size());
  EXPECT_EQ(responses[0], responses_.front().response);

  responses_.clear();

  // No actual request - should be a cache hit.
  EXPECT_TRUE(download_manager_.StartQueryRequest(
      ToRawPointerVector(form_structures0)));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 2);
  // Data is available immediately from cache - no over-the-wire trip.
  ASSERT_EQ(1U, responses_.size());
  EXPECT_EQ(responses[0], responses_.front().response);
  responses_.clear();

  // Request with id 1.
  EXPECT_TRUE(download_manager_.StartQueryRequest(
      ToRawPointerVector(form_structures1)));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 3);
  // No responses yet
  EXPECT_EQ(0U, responses_.size());

  request = test_url_loader_factory_.GetPendingRequest(1);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, responses[1]);
  ASSERT_EQ(1U, responses_.size());
  EXPECT_EQ(responses[1], responses_.front().response);

  responses_.clear();

  // Request with id 2.
  EXPECT_TRUE(download_manager_.StartQueryRequest(
      ToRawPointerVector(form_structures2)));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 4);

  request = test_url_loader_factory_.GetPendingRequest(2);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, responses[2]);
  ASSERT_EQ(1U, responses_.size());
  EXPECT_EQ(responses[2], responses_.front().response);

  responses_.clear();

  // No actual requests - should be a cache hit.
  EXPECT_TRUE(download_manager_.StartQueryRequest(
      ToRawPointerVector(form_structures1)));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 5);

  EXPECT_TRUE(download_manager_.StartQueryRequest(
      ToRawPointerVector(form_structures2)));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 6);

  ASSERT_EQ(2U, responses_.size());
  EXPECT_EQ(responses[1], responses_.front().response);
  EXPECT_EQ(responses[2], responses_.back().response);
  responses_.clear();

  // The first structure should have expired.
  // Request with id 3.
  EXPECT_TRUE(download_manager_.StartQueryRequest(
      ToRawPointerVector(form_structures0)));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 7);
  // No responses yet
  EXPECT_EQ(0U, responses_.size());

  request = test_url_loader_factory_.GetPendingRequest(3);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, responses[0]);
  ASSERT_EQ(1U, responses_.size());
  EXPECT_EQ(responses[0], responses_.front().response);
}

namespace {

enum ServerCommuncationMode {
  DISABLED,
  FINCHED_URL,
  COMMAND_LINE_URL,
  DEFAULT_URL
};

class AutofillServerCommunicationTest
    : public AutofillDownloadManager::Observer,
      public testing::TestWithParam<ServerCommuncationMode> {
 protected:
  void SetUp() override {
    testing::TestWithParam<ServerCommuncationMode>::SetUp();

    pref_service_ = test::PrefServiceForTesting();

    scoped_feature_list_1_.InitWithFeatures(
        // Enabled
        {features::kAutofillCacheQueryResponses,
         features::kAutofillUploadThrottling},
        // Disabled
        {});

    // Setup the server.
    server_.RegisterRequestHandler(
        base::BindRepeating(&AutofillServerCommunicationTest::RequestHandler,
                            base::Unretained(this)));
    ASSERT_TRUE(server_.Start());

    GURL autofill_server_url(server_.base_url().Resolve("/tbproxy/af/"));
    ASSERT_TRUE(autofill_server_url.is_valid());

    // Intialize the autofill driver.
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
            nullptr /* network_service */, true /* is_trusted */);
    driver_ = std::make_unique<TestAutofillDriver>();
    driver_->SetSharedURLLoaderFactory(shared_url_loader_factory_);
    driver_->SetNetworkIsolationKey(
        net::NetworkIsolationKey(url::Origin::Create(GURL("https://abc.com")),
                                 url::Origin::Create(GURL("https://xyz.com"))));

    // Configure the autofill server communications channel.
    switch (GetParam()) {
      case DISABLED:
        scoped_feature_list_2_.InitAndDisableFeature(
            features::kAutofillServerCommunication);
        break;
      case FINCHED_URL:
        scoped_feature_list_2_.InitAndEnableFeatureWithParameters(
            features::kAutofillServerCommunication,
            {{switches::kAutofillServerURL, autofill_server_url.spec()}});
        break;
      case COMMAND_LINE_URL:
        scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
            switches::kAutofillServerURL, autofill_server_url.spec());
        FALLTHROUGH;
      case DEFAULT_URL:
        scoped_feature_list_2_.InitAndEnableFeature(
            features::kAutofillServerCommunication);
        break;
      default:
        ASSERT_TRUE(false);
    }
  }

  void TearDown() override {
    if (server_.Started())
      ASSERT_TRUE(server_.ShutdownAndWaitUntilComplete());

    auto* variations_http_header_provider =
        variations::VariationsHttpHeaderProvider::GetInstance();
    if (variations_http_header_provider != nullptr)
      variations_http_header_provider->ResetForTesting();
  }

  // AutofillDownloadManager::Observer implementation.
  void OnLoadedServerPredictions(
      std::string /* response_xml */,
      const std::vector<std::string>& /*form_signatures */) override {
    ASSERT_TRUE(run_loop_);
    run_loop_->QuitWhenIdle();
  }

  void OnUploadedPossibleFieldTypes() override {
    ASSERT_TRUE(run_loop_);
    run_loop_->QuitWhenIdle();
  }

  // Helper to extract the value of a query param. Returns "*** not found ***"
  // if the requested query param is not in the query string.
  std::string GetQueryParam(const std::string& query_str,
                            const std::string& param_name) {
    url::Component query(0, query_str.length());
    url::Component key, value;
    while (url::ExtractQueryKeyValue(query_str.c_str(), &query, &key, &value)) {
      base::StringPiece key_string(query_str.c_str() + key.begin, key.len);
      base::StringPiece param_text(query_str.c_str() + value.begin, value.len);
      std::string param_value;
      if (key_string == param_name &&
          base::Base64UrlDecode(param_text,
                                base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                                &param_value)) {
        return param_value;
      }
    }
    return "*** not found ***";
  }

  std::unique_ptr<HttpResponse> RequestHandler(const HttpRequest& request) {
    GURL absolute_url = server_.GetURL(request.relative_url);
    ++call_count_;

    if (absolute_url.path() == "/tbproxy/af/query") {
      payloads_.push_back(!request.content.empty()
                              ? request.content
                              : GetQueryParam(absolute_url.query(), "q"));
      AutofillQueryResponseContents proto;
      proto.add_field()->set_overall_type_prediction(NAME_FIRST);

      auto response = std::make_unique<BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content(proto.SerializeAsString());
      response->set_content_type("text/proto");
      response->AddCustomHeader(
          "Cache-Control",
          base::StringPrintf("max-age=%" PRId64,
                             base::TimeDelta::FromMilliseconds(
                                 cache_expiration_in_milliseconds_)
                                 .InSeconds()));
      return response;
    }

    if (absolute_url.path() == "/tbproxy/af/upload") {
      payloads_.push_back(request.content);
      auto response = std::make_unique<BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      return response;
    }

    return nullptr;
  }

  bool SendQueryRequest(
      const std::vector<std::unique_ptr<FormStructure>>& form_structures) {
    EXPECT_EQ(run_loop_, nullptr);
    run_loop_ = std::make_unique<base::RunLoop>();

    ScopedActiveAutofillExperiments scoped_active_autofill_experiments;
    AutofillDownloadManager download_manager(driver_.get(), this);
    bool succeeded =
        download_manager.StartQueryRequest(ToRawPointerVector(form_structures));
    if (succeeded)
      run_loop_->Run();
    run_loop_.reset();
    return succeeded;
  }

  bool SendUploadRequest(const FormStructure& form,
                         bool form_was_autofilled,
                         const ServerFieldTypeSet& available_field_types,
                         const std::string& login_form_signature,
                         bool observed_submission) {
    EXPECT_EQ(run_loop_, nullptr);
    run_loop_ = std::make_unique<base::RunLoop>();

    ScopedActiveAutofillExperiments scoped_active_autofill_experiments;
    AutofillDownloadManager download_manager(driver_.get(), this);
    bool succeeded = download_manager.StartUploadRequest(
        form, form_was_autofilled, available_field_types, login_form_signature,
        observed_submission, pref_service_.get());
    if (succeeded)
      run_loop_->Run();
    run_loop_.reset();
    return succeeded;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  base::test::ScopedCommandLine scoped_command_line_;
  base::test::ScopedFeatureList scoped_feature_list_1_;
  base::test::ScopedFeatureList scoped_feature_list_2_;
  EmbeddedTestServer server_;
  int cache_expiration_in_milliseconds_ = 100000;
  std::unique_ptr<base::RunLoop> run_loop_;
  size_t call_count_ = 0;
  scoped_refptr<network::TestSharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<TestAutofillDriver> driver_;
  std::unique_ptr<PrefService> pref_service_;
  std::vector<std::string> payloads_;
};

}  // namespace

TEST_P(AutofillServerCommunicationTest, IsEnabled) {
  AutofillDownloadManager download_manager(driver_.get(), this);
  EXPECT_EQ(download_manager.IsEnabled(), GetParam() != DISABLED);
}

TEST_P(AutofillServerCommunicationTest, Query) {
  FormData form;
  FormFieldData field;

  field.label = UTF8ToUTF16("First Name:");
  field.name = UTF8ToUTF16("firstname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  std::vector<std::unique_ptr<FormStructure>> form_structures;
  form_structures.push_back(std::make_unique<FormStructure>(form));

  EXPECT_EQ(GetParam() != DISABLED, SendQueryRequest(form_structures));
}

TEST_P(AutofillServerCommunicationTest, Upload) {
  FormData form;
  FormFieldData field;

  field.label = UTF8ToUTF16("First Name:");
  field.name = UTF8ToUTF16("firstname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("Last Name:");
  field.name = UTF8ToUTF16("lastname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("Email:");
  field.name = UTF8ToUTF16("email");
  field.form_control_type = "text";
  form.fields.push_back(field);

  AutofillDownloadManager download_manager(driver_.get(), this);
  EXPECT_EQ(GetParam() != DISABLED,
            SendUploadRequest(FormStructure(form), true, {}, "", true));
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
  FormFieldData field;
  field.label = UTF8ToUTF16("First Name:");
  field.name = UTF8ToUTF16("firstname");
  field.form_control_type = "text";

  FormData form;
  form.fields.push_back(field);

  std::vector<std::unique_ptr<FormStructure>> form_structures;
  form_structures.push_back(std::make_unique<FormStructure>(form));

  // Query for the form. This should go to the embedded server.
  {
    SCOPED_TRACE("First Query");
    base::HistogramTester histogram;
    call_count_ = 0;
    ASSERT_TRUE(SendQueryRequest(form_structures));
    EXPECT_EQ(1u, call_count_);
    histogram.ExpectBucketCount("Autofill.ServerQueryResponse",
                                AutofillMetrics::QUERY_SENT, 1);
    histogram.ExpectBucketCount("Autofill.Query.Method", METHOD_GET, 1);
    histogram.ExpectBucketCount("Autofill.Query.WasInCache", CACHE_MISS, 1);
  }

  // Query again. This should go to the cache, since the max-age for the cached
  // response is 2 days.
  {
    SCOPED_TRACE("Second Query");
    base::HistogramTester histogram;
    call_count_ = 0;
    ASSERT_TRUE(SendQueryRequest(form_structures));
    EXPECT_EQ(0u, call_count_);
    histogram.ExpectBucketCount("Autofill.ServerQueryResponse",
                                AutofillMetrics::QUERY_SENT, 1);
    histogram.ExpectBucketCount("Autofill.Query.Method", METHOD_GET, 1);
    histogram.ExpectBucketCount("Autofill.Query.WasInCache", CACHE_HIT, 1);
  }
}

TEST_P(AutofillQueryTest, SendsExperiment) {
  FormFieldData field;
  field.label = UTF8ToUTF16("First Name:");
  field.name = UTF8ToUTF16("firstname");
  field.form_control_type = "text";

  FormData form;
  form.fields.push_back(field);

  std::vector<std::unique_ptr<FormStructure>> form_structures;
  form_structures.push_back(std::make_unique<FormStructure>(form));

  // Query for the form. This should go to the embedded server.
  {
    SCOPED_TRACE("First Query");
    base::HistogramTester histogram;
    call_count_ = 0;
    ASSERT_TRUE(SendQueryRequest(form_structures));
    EXPECT_EQ(1u, call_count_);
    histogram.ExpectBucketCount("Autofill.ServerQueryResponse",
                                AutofillMetrics::QUERY_SENT, 1);
    histogram.ExpectBucketCount("Autofill.Query.Method", METHOD_GET, 1);
    histogram.ExpectBucketCount("Autofill.Query.WasInCache", CACHE_MISS, 1);
  }

  // Add experiment/variation idd from the range reserved for autofill.
  auto* variations_http_header_provider =
      variations::VariationsHttpHeaderProvider::GetInstance();
  ASSERT_TRUE(variations_http_header_provider != nullptr);
  variations_http_header_provider->ForceVariationIds(
      {"t3314883", "t3312923", "t3314885"},  // first two valid, out-of-order
      {});

  // Query again. This should go to the embedded server since it's not the same
  // as the previously cached query.
  {
    SCOPED_TRACE("Second Query");
    base::HistogramTester histogram;
    call_count_ = 0;
    payloads_.clear();
    ASSERT_TRUE(SendQueryRequest(form_structures));
    EXPECT_EQ(1u, call_count_);
    histogram.ExpectBucketCount("Autofill.ServerQueryResponse",
                                AutofillMetrics::QUERY_SENT, 1);
    histogram.ExpectBucketCount("Autofill.Query.Method", METHOD_GET, 1);
    histogram.ExpectBucketCount("Autofill.Query.WasInCache", CACHE_MISS, 1);

    ASSERT_EQ(1u, payloads_.size());
    AutofillQueryContents query_contents;
    ASSERT_TRUE(query_contents.ParseFromString(payloads_[0]));
    ASSERT_EQ(2, query_contents.experiments_size());
    EXPECT_EQ(3312923, query_contents.experiments(0));
    EXPECT_EQ(3314883, query_contents.experiments(1));
  }

  // Query a third time (will experiments still enabled). This should go to the
  // cache.
  {
    SCOPED_TRACE("Third Query");
    base::HistogramTester histogram;
    call_count_ = 0;
    ASSERT_TRUE(SendQueryRequest(form_structures));
    EXPECT_EQ(0u, call_count_);
    histogram.ExpectBucketCount("Autofill.ServerQueryResponse",
                                AutofillMetrics::QUERY_SENT, 1);
    histogram.ExpectBucketCount("Autofill.Query.Method", METHOD_GET, 1);
    histogram.ExpectBucketCount("Autofill.Query.WasInCache", CACHE_HIT, 1);
  }
}

TEST_P(AutofillQueryTest, ExpiredCacheInResponse) {
  FormFieldData field;
  field.label = UTF8ToUTF16("First Name:");
  field.name = UTF8ToUTF16("firstname");
  field.form_control_type = "text";

  FormData form;
  form.fields.push_back(field);

  std::vector<std::unique_ptr<FormStructure>> form_structures;
  form_structures.push_back(std::make_unique<FormStructure>(form));

  // Set the cache expiration interval to 0.
  cache_expiration_in_milliseconds_ = 0;

  // Query for the form. This should go to the embedded server.
  {
    SCOPED_TRACE("First Query");
    base::HistogramTester histogram;
    call_count_ = 0;
    ASSERT_TRUE(SendQueryRequest(form_structures));
    EXPECT_EQ(1u, call_count_);
    histogram.ExpectBucketCount("Autofill.ServerQueryResponse",
                                AutofillMetrics::QUERY_SENT, 1);
    histogram.ExpectBucketCount("Autofill.Query.Method", METHOD_GET, 1);
    histogram.ExpectBucketCount("Autofill.Query.WasInCache", CACHE_MISS, 1);
  }

  // The cache entry had a max age of 0 ms, so delaying only a few milliseconds
  // ensures the cache expires and no request are served by cached content
  // (ie this should go to the embedded server).
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(100));
  run_loop.Run();

  {
    SCOPED_TRACE("Second Query");
    base::HistogramTester histogram;
    call_count_ = 0;
    ASSERT_TRUE(SendQueryRequest(form_structures));
    EXPECT_EQ(1u, call_count_);
    histogram.ExpectBucketCount("Autofill.ServerQueryResponse",
                                AutofillMetrics::QUERY_SENT, 1);
    histogram.ExpectBucketCount("Autofill.Query.Method", METHOD_GET, 1);
    histogram.ExpectBucketCount("Autofill.Query.WasInCache", CACHE_MISS, 1);
  }
}

TEST_P(AutofillQueryTest, RichMetadata_Enabled) {
  // Initialize a form. Note that this state is post-parse.
  FormData form;
  form.url = GURL("https://origin.com");
  form.action = GURL("https://origin.com/submit-me");
  form.id_attribute = UTF8ToUTF16("form-id-attribute");
  form.name_attribute = UTF8ToUTF16("form-name-attribute");
  form.name = form.name_attribute;

  // Add field 0.
  FormFieldData field;
  field.id_attribute = UTF8ToUTF16("field-id-attribute-1");
  field.name_attribute = UTF8ToUTF16("field-name-attribute-1");
  field.name = field.name_attribute;
  field.label = UTF8ToUTF16("field-label");
  field.aria_label = UTF8ToUTF16("field-aria-label");
  field.aria_description = UTF8ToUTF16("field-aria-description");
  field.form_control_type = "text";
  field.css_classes = UTF8ToUTF16("field-css-classes");
  field.placeholder = UTF8ToUTF16("field-placeholder");
  form.fields.push_back(field);

  // Add field 1.
  field.id_attribute = UTF8ToUTF16("field-id-attribute-2");
  field.name_attribute = UTF8ToUTF16("field-name-attribute-2");
  field.name = field.name_attribute;
  field.label = UTF8ToUTF16("field-label");
  field.aria_label = UTF8ToUTF16("field-aria-label");
  field.aria_description = UTF8ToUTF16("field-aria-description");
  field.form_control_type = "text";
  field.css_classes = UTF8ToUTF16("field-css-classes");
  field.placeholder = UTF8ToUTF16("field-placeholder");
  form.fields.push_back(field);

  // Add field 2.
  field.id_attribute = UTF8ToUTF16("field-id-attribute-3");
  field.name_attribute = UTF8ToUTF16("field-name-attribute-3");
  field.name = field.name_attribute;
  field.label = UTF8ToUTF16("field-label");
  field.aria_label = UTF8ToUTF16("field-aria-label");
  field.aria_description = UTF8ToUTF16("field-aria-description");
  field.form_control_type = "text";
  field.css_classes = UTF8ToUTF16("field-css-classes");
  field.placeholder = UTF8ToUTF16("field-placeholder");
  form.fields.push_back(field);

  // Setup the form structures to query.
  AutofillDownloadManager download_manager(driver_.get(), this);
  std::vector<std::unique_ptr<FormStructure>> form_structures;
  form_structures.push_back(std::make_unique<FormStructure>(form));

  // Turn on rich query encoding.
  form_structures.front()->set_is_rich_query_enabled(true);

  // Generate a query request.
  ASSERT_TRUE(SendQueryRequest(form_structures));
  EXPECT_EQ(1u, call_count_);

  // We should have intercepted exactly on query request. Parse it.
  ASSERT_EQ(1u, payloads_.size());
  AutofillQueryContents query;
  ASSERT_TRUE(query.ParseFromString(payloads_.front()));

  // Validate that we have one form in the query.
  ASSERT_EQ(query.form_size(), 1);
  const auto& query_form = query.form(0);

  // The form should have metadata, and the metadata value should be equal
  // those initialized above.
  ASSERT_TRUE(query_form.has_form_metadata());
  EXPECT_EQ(UTF8ToUTF16(query_form.form_metadata().id().encoded_bits()),
            form.id_attribute);
  EXPECT_EQ(UTF8ToUTF16(query_form.form_metadata().name().encoded_bits()),
            form.name_attribute);

  // The form should have 3 fields, and their metadata value should be equal
  // those initialized above.
  ASSERT_EQ(3, query_form.field_size());
  ASSERT_EQ(static_cast<int>(form.fields.size()), query_form.field_size());
  for (int i = 0; i < query_form.field_size(); ++i) {
    const auto& query_field = query_form.field(i);
    const auto& form_field = form.fields[i];
    ASSERT_TRUE(query_field.has_field_metadata());
    const auto& meta = query_field.field_metadata();
    EXPECT_EQ(UTF8ToUTF16(meta.id().encoded_bits()), form_field.id_attribute);
    EXPECT_EQ(UTF8ToUTF16(meta.name().encoded_bits()),
              form_field.name_attribute);
    EXPECT_EQ(meta.type().encoded_bits(), form_field.form_control_type);
    EXPECT_EQ(UTF8ToUTF16(meta.label().encoded_bits()), form_field.label);
    EXPECT_EQ(UTF8ToUTF16(meta.aria_label().encoded_bits()),
              form_field.aria_label);
    EXPECT_EQ(UTF8ToUTF16(meta.aria_description().encoded_bits()),
              form_field.aria_description);
    EXPECT_EQ(UTF8ToUTF16(meta.css_class().encoded_bits()),
              form_field.css_classes);
    EXPECT_EQ(UTF8ToUTF16(meta.placeholder().encoded_bits()),
              form_field.placeholder);
  }
}

TEST_P(AutofillQueryTest, RichMetadata_Disabled) {
  // Initialize a form. Note that this state is post-parse.
  FormData form;
  form.url = GURL("https://origin.com");
  form.action = GURL("https://origin.com/submit-me");
  form.id_attribute = UTF8ToUTF16("form-id-attribute");
  form.name_attribute = UTF8ToUTF16("form-name-attribute");
  form.name = form.name_attribute;

  // Add field 0.
  FormFieldData field;
  field.id_attribute = UTF8ToUTF16("field-id-attribute-1");
  field.name_attribute = UTF8ToUTF16("field-name-attribute-1");
  field.name = field.name_attribute;
  field.label = UTF8ToUTF16("field-label");
  field.aria_label = UTF8ToUTF16("field-aria-label");
  field.aria_description = UTF8ToUTF16("field-aria-description");
  field.form_control_type = "text";
  field.css_classes = UTF8ToUTF16("field-css-classes");
  field.placeholder = UTF8ToUTF16("field-placeholder");
  form.fields.push_back(field);

  // Add field 1.
  field.id_attribute = UTF8ToUTF16("field-id-attribute-2");
  field.name_attribute = UTF8ToUTF16("field-name-attribute-2");
  field.name = field.name_attribute;
  field.label = UTF8ToUTF16("field-label");
  field.aria_label = UTF8ToUTF16("field-aria-label");
  field.aria_description = UTF8ToUTF16("field-aria-description");
  field.form_control_type = "text";
  field.css_classes = UTF8ToUTF16("field-css-classes");
  field.placeholder = UTF8ToUTF16("field-placeholder");
  form.fields.push_back(field);

  // Add field 2.
  field.id_attribute = UTF8ToUTF16("field-id-attribute-3");
  field.name_attribute = UTF8ToUTF16("field-name-attribute-3");
  field.name = field.name_attribute;
  field.label = UTF8ToUTF16("field-label");
  field.aria_label = UTF8ToUTF16("field-aria-label");
  field.aria_description = UTF8ToUTF16("field-aria-description");
  field.form_control_type = "text";
  field.css_classes = UTF8ToUTF16("field-css-classes");
  field.placeholder = UTF8ToUTF16("field-placeholder");
  form.fields.push_back(field);

  // Setup the form structures to query.
  AutofillDownloadManager download_manager(driver_.get(), this);
  std::vector<std::unique_ptr<FormStructure>> form_structures;
  form_structures.push_back(std::make_unique<FormStructure>(form));

  // Turn off rich query encoding.
  form_structures.front()->set_is_rich_query_enabled(false);

  // Generate a query request.
  ASSERT_TRUE(SendQueryRequest(form_structures));
  EXPECT_EQ(1u, call_count_);

  // We should have intercepted exactly on query request. Parse it.
  ASSERT_EQ(1u, payloads_.size());
  AutofillQueryContents query;
  ASSERT_TRUE(query.ParseFromString(payloads_.front()));

  // Validate that we have one form in the query.
  ASSERT_EQ(query.form_size(), 1);
  const auto& query_form = query.form(0);

  // There should be no encoded metadata for the form.
  EXPECT_FALSE(query_form.has_form_metadata());

  // There should be three fields, none of which have encoded metadata.
  ASSERT_EQ(3, query_form.field_size());
  ASSERT_EQ(static_cast<int>(form.fields.size()), query_form.field_size());
  for (int i = 0; i < query_form.field_size(); ++i) {
    const auto& query_field = query_form.field(i);
    EXPECT_FALSE(query_field.has_field_metadata());
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
  local_feature.InitAndEnableFeature(features::kAutofillMetadataUploads);

  FormData form;
  form.url = GURL("https://origin.com");
  form.action = GURL("https://origin.com/submit-me");
  form.id_attribute = UTF8ToUTF16("form-id_attribute");
  form.name_attribute = UTF8ToUTF16("form-id_attribute");
  form.name = form.name_attribute;

  FormFieldData field;
  field.id_attribute = UTF8ToUTF16("field-id-attribute-1");
  field.name_attribute = UTF8ToUTF16("field-name-attribute-1");
  field.name = field.name_attribute;
  field.label = UTF8ToUTF16("field-label");
  field.aria_label = UTF8ToUTF16("field-aria-label");
  field.aria_description = UTF8ToUTF16("field-aria-descriptionm");
  field.form_control_type = "text";
  field.css_classes = UTF8ToUTF16("field-css-classes");
  field.placeholder = UTF8ToUTF16("field-placeholder");
  form.fields.push_back(field);

  field.id_attribute = UTF8ToUTF16("field-id-attribute-2");
  field.name_attribute = UTF8ToUTF16("field-name-attribute-2");
  field.name = field.name_attribute;
  field.label = UTF8ToUTF16("field-label");
  field.aria_label = UTF8ToUTF16("field-aria-label");
  field.aria_description = UTF8ToUTF16("field-aria-descriptionm");
  field.form_control_type = "text";
  field.css_classes = UTF8ToUTF16("field-css-classes");
  field.placeholder = UTF8ToUTF16("field-placeholder");
  form.fields.push_back(field);

  field.id_attribute = UTF8ToUTF16("field-id-attribute-3");
  field.name_attribute = UTF8ToUTF16("field-name-attribute-3");
  field.name = field.name_attribute;
  field.label = UTF8ToUTF16("field-label");
  field.aria_label = UTF8ToUTF16("field-aria-label");
  field.aria_description = UTF8ToUTF16("field-aria-descriptionm");
  field.form_control_type = "text";
  field.css_classes = UTF8ToUTF16("field-css-classes");
  field.placeholder = UTF8ToUTF16("field-placeholder");
  form.fields.push_back(field);

  AutofillDownloadManager download_manager(driver_.get(), this);
  FormStructure form_structure(form);
  form_structure.set_page_language("fr-ca");

  for (int i = 0; i <= static_cast<int>(SubmissionSource::kMaxValue); ++i) {
    base::HistogramTester histogram_tester;
    auto submission_source = static_cast<SubmissionSource>(i);
    SCOPED_TRACE(testing::Message()
                 << "submission source = " << submission_source);
    form_structure.set_submission_source(submission_source);
    form_structure.set_randomized_encoder(
        RandomizedEncoder::Create(pref_service_.get()));

    payloads_.clear();

    // The first attempt should succeed.
    EXPECT_TRUE(SendUploadRequest(form_structure, true, {}, "", true));

    // The second attempt should always fail.
    EXPECT_FALSE(SendUploadRequest(form_structure, true, {}, "", true));

    // One upload was sent.
    histogram_tester.ExpectBucketCount("Autofill.UploadEvent", 1, 1);
    histogram_tester.ExpectBucketCount(
        AutofillMetrics::SubmissionSourceToUploadEventMetric(submission_source),
        1, 1);

    // Three encoding events should be sent.
    histogram_tester.ExpectUniqueSample("Autofill.Upload.MetadataConfigIsValid",
                                        true, 1);

    ASSERT_EQ(1u, payloads_.size());
    AutofillUploadContents upload;
    ASSERT_TRUE(upload.ParseFromString(payloads_.front()));
    EXPECT_EQ(upload.language(), form_structure.page_language());
    ASSERT_TRUE(upload.has_randomized_form_metadata());
    EXPECT_TRUE(upload.randomized_form_metadata().has_id());
    EXPECT_TRUE(upload.randomized_form_metadata().has_name());
    EXPECT_EQ(3, upload.field_size());
    for (const auto& f : upload.field()) {
      ASSERT_TRUE(f.has_randomized_field_metadata());
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

TEST_P(AutofillUploadTest, Throttling) {
  ASSERT_NE(DISABLED, GetParam());

  FormData form;
  FormFieldData field;

  field.label = UTF8ToUTF16("First Name:");
  field.name = UTF8ToUTF16("firstname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("Last Name:");
  field.name = UTF8ToUTF16("lastname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("Email:");
  field.name = UTF8ToUTF16("email");
  field.form_control_type = "text";
  form.fields.push_back(field);

  AutofillDownloadManager download_manager(driver_.get(), this);
  FormStructure form_structure(form);
  for (int i = 0; i <= static_cast<int>(SubmissionSource::kMaxValue); ++i) {
    base::HistogramTester histogram_tester;
    auto submission_source = static_cast<SubmissionSource>(i);
    SCOPED_TRACE(testing::Message()
                 << "submission source = " << submission_source);
    form_structure.set_submission_source(submission_source);

    // The first attempt should succeed.
    EXPECT_TRUE(SendUploadRequest(form_structure, true, {}, "", true));

    // The second attempt should always fail.
    EXPECT_FALSE(SendUploadRequest(form_structure, true, {}, "", true));

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

TEST_P(AutofillUploadTest, ThrottlingDisabled) {
  ASSERT_NE(DISABLED, GetParam());
  base::test::ScopedFeatureList local_feature;
  local_feature.InitWithFeatures(
      // Enabled.
      {},
      // Disabled
      {features::kAutofillUploadThrottling,
       features::kAutofillEnforceMinRequiredFieldsForUpload});

  FormData form;
  FormData small_form;
  FormFieldData field;

  field.label = UTF8ToUTF16("First Name:");
  field.name = UTF8ToUTF16("firstname");
  field.form_control_type = "text";
  form.fields.push_back(field);
  small_form.fields.push_back(field);

  field.label = UTF8ToUTF16("Last Name:");
  field.name = UTF8ToUTF16("lastname");
  field.form_control_type = "text";
  form.fields.push_back(field);
  small_form.fields.push_back(field);

  field.label = UTF8ToUTF16("Email:");
  field.name = UTF8ToUTF16("email");
  field.form_control_type = "text";
  form.fields.push_back(field);

  AutofillDownloadManager download_manager(driver_.get(), this);
  FormStructure form_structure(form);
  FormStructure small_form_structure(small_form);

  for (int i = 0; i <= static_cast<int>(SubmissionSource::kMaxValue); ++i) {
    base::HistogramTester histogram_tester;
    auto submission_source = static_cast<SubmissionSource>(i);
    SCOPED_TRACE(testing::Message()
                 << "submission source = " << submission_source);
    form_structure.set_submission_source(submission_source);
    small_form_structure.set_submission_source(submission_source);

    payloads_.clear();

    // The first attempt should succeed.
    EXPECT_TRUE(SendUploadRequest(form_structure, true, {}, "", true));

    // The second attempt should also succeed
    EXPECT_TRUE(SendUploadRequest(form_structure, true, {}, "", true));

    // The third attempt should also succeed
    EXPECT_TRUE(SendUploadRequest(form_structure, true, {}, "", true));

    // The first small form attempt should succeed
    EXPECT_TRUE(SendUploadRequest(small_form_structure, true, {}, "", true));

    // The second small form attempt should be throttled, even if throttling
    // is disabled.
    EXPECT_FALSE(SendUploadRequest(small_form_structure, true, {}, "", true));

    // All uploads were allowed..
    histogram_tester.ExpectBucketCount("Autofill.UploadEvent", 1, 4);
    histogram_tester.ExpectBucketCount(
        AutofillMetrics::SubmissionSourceToUploadEventMetric(submission_source),
        1, 4);
    histogram_tester.ExpectBucketCount(
        AutofillMetrics::SubmissionSourceToUploadEventMetric(submission_source),
        0, 1);

    // The last middle two uploads were marked as throttle-able.
    ASSERT_EQ(4u, payloads_.size());
    for (size_t i = 0; i < payloads_.size(); ++i) {
      AutofillUploadContents upload_contents;
      ASSERT_TRUE(upload_contents.ParseFromString(payloads_[i]));
      EXPECT_EQ(upload_contents.was_throttleable(), (i == 1 || i == 2))
          << "Wrong was_throttleable value for upload " << i;
      EXPECT_FALSE(upload_contents.has_randomized_form_metadata());
      for (const auto& field : upload_contents.field()) {
        EXPECT_FALSE(field.has_randomized_field_metadata());
      }
    }
  }
}

TEST_P(AutofillUploadTest, PeriodicReset) {
  ASSERT_NE(DISABLED, GetParam());

  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndEnableFeatureWithParameters(
      features::kAutofillUploadThrottling,
      {{switches::kAutofillUploadThrottlingPeriodInDays, "16"}});

  FormData form;
  FormFieldData field;

  field.label = UTF8ToUTF16("First Name:");
  field.name = UTF8ToUTF16("firstname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("Last Name:");
  field.name = UTF8ToUTF16("lastname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("Email:");
  field.name = UTF8ToUTF16("email");
  field.form_control_type = "text";
  form.fields.push_back(field);

  AutofillDownloadManager download_manager(driver_.get(), this);
  SubmissionSource submission_source = SubmissionSource::FORM_SUBMISSION;

  FormStructure form_structure(form);
  form_structure.set_submission_source(submission_source);

  base::HistogramTester histogram_tester;

  TestAutofillClock test_clock;
  test_clock.SetNow(AutofillClock::Now());

  // The first attempt should succeed.
  EXPECT_TRUE(SendUploadRequest(form_structure, true, {}, "", true));

  // Advance the clock, but not past the reset period. The pref won't reset,
  // so the upload should not be sent.
  test_clock.Advance(base::TimeDelta::FromDays(15));
  EXPECT_FALSE(SendUploadRequest(form_structure, true, {}, "", true));

  // Advance the clock beyond the reset period. The pref should be reset and
  // the upload should succeed.
  test_clock.Advance(base::TimeDelta::FromDays(2));  // Total = 29
  EXPECT_TRUE(SendUploadRequest(form_structure, true, {}, "", true));

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

TEST_P(AutofillUploadTest, ResetOnClearUploadHisotry) {
  ASSERT_NE(DISABLED, GetParam());

  FormData form;
  FormFieldData field;

  field.label = UTF8ToUTF16("First Name:");
  field.name = UTF8ToUTF16("firstname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("Last Name:");
  field.name = UTF8ToUTF16("lastname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = UTF8ToUTF16("Email:");
  field.name = UTF8ToUTF16("email");
  field.form_control_type = "text";
  form.fields.push_back(field);

  AutofillDownloadManager download_manager(driver_.get(), this);
  SubmissionSource submission_source = SubmissionSource::FORM_SUBMISSION;

  FormStructure form_structure(form);
  form_structure.set_submission_source(submission_source);

  base::HistogramTester histogram_tester;

  TestAutofillClock test_clock;
  test_clock.SetNow(AutofillClock::Now());

  // The first attempt should succeed.
  EXPECT_TRUE(SendUploadRequest(form_structure, true, {}, "", true));

  // Clear the upload throttling history.
  AutofillDownloadManager::ClearUploadHistory(pref_service_.get());
  EXPECT_TRUE(SendUploadRequest(form_structure, true, {}, "", true));

  // Two uploads were sent.
  histogram_tester.ExpectBucketCount("Autofill.UploadEvent", 1, 2);
  histogram_tester.ExpectBucketCount(
      AutofillMetrics::SubmissionSourceToUploadEventMetric(submission_source),
      1, 2);
}

// Note that we omit DEFAULT_URL from the test params. We don't actually want
// the tests to hit the production server. We also excluded DISABLED, since
// these tests exercise "enabled" functionality.
INSTANTIATE_TEST_SUITE_P(All,
                         AutofillUploadTest,
                         ::testing::Values(FINCHED_URL, COMMAND_LINE_URL));

}  // namespace autofill
