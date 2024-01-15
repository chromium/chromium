// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/ipp_validator.h"

#include <cups/cups.h>

#include <set>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "chrome/services/cups_proxy/fake_cups_proxy_service_delegate.h"
#include "chrome/services/cups_proxy/public/cpp/cups_util.h"
#include "chrome/services/cups_proxy/public/cpp/ipp_messages.h"
#include "printing/backend/cups_ipp_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cups_proxy {
namespace {

// Mojom using statements to remove chrome::mojom everywhere...
using ipp_parser::mojom::IppAttributePtr;
using ipp_parser::mojom::IppMessagePtr;
using ipp_parser::mojom::IppRequestPtr;

using Printer = chromeos::Printer;

std::string EncodeEndpointForPrinterId(std::string printer_id) {
  return "/printers/" + printer_id;
}

// Fake backend for CupsProxyServiceDelegate.
class FakeServiceDelegate : public FakeCupsProxyServiceDelegate {
 public:
  FakeServiceDelegate() = default;
  ~FakeServiceDelegate() override = default;

  void AddPrinter(const std::string& printer_id) {
    known_printers_.insert(printer_id);
  }

  std::optional<Printer> GetPrinter(const std::string& id) override {
    if (!base::Contains(known_printers_, id)) {
      return std::nullopt;
    }

    return Printer(id);
  }

 private:
  std::set<std::string> known_printers_;
};

class IppValidatorTest : public testing::Test {
 public:
  IppValidatorTest() {
    delegate_ = std::make_unique<FakeServiceDelegate>();
    ipp_validator_ = std::make_unique<IppValidator>(delegate_.get());
  }

  ~IppValidatorTest() override = default;

  std::optional<IppRequest> RunValidateIppRequest(
      const IppRequestPtr& request) {
    return ipp_validator_->ValidateIppRequest(request.Clone());
  }

 protected:
  // Fake delegate driving the IppValidator.
  std::unique_ptr<FakeServiceDelegate> delegate_;

  // The manager being tested. This must be declared, and thus initialized,
  // after the fakes.
  std::unique_ptr<IppValidator> ipp_validator_;
};

IppAttributePtr BuildAttributePtr(std::string name,
                                  ipp_tag_t group_tag,
                                  ipp_tag_t value_tag) {
  auto ret = ipp_parser::mojom::IppAttribute::New();
  ret->name = name;
  ret->group_tag = group_tag;
  ret->value_tag = value_tag;
  return ret;
}

// Returns a mojom representation of a standard IPP request.
IppRequestPtr GetBasicIppRequest() {
  IppRequestPtr ret = ipp_parser::mojom::IppRequest::New();

  // Request line.
  ret->method = "POST";
  ret->endpoint = "/";
  ret->http_version = "HTTP/1.1";

  // Map of Http headers.
  ret->headers = base::flat_map<ipp_converter::HttpHeader::first_type,
                                ipp_converter::HttpHeader::second_type>{
      {"Content-Length", "72"},
      {"Content-Type", "application/ipp"},
      {"Date", "Thu, 04 Oct 2018 20:25:59 GMT"},
      {"Host", "localhost:0"},
      {"User-Agent",
       "CUPS/2.3b1 (Linux 4.4.159-15303-g65f4b5a7b3d3; i686) IPP/2.0"}};

  // IppMessage.
  IppMessagePtr ipp_message = ipp_parser::mojom::IppMessage::New();
  ipp_message->major_version = 2;
  ipp_message->minor_version = 0;
  ipp_message->operation_id = IPP_OP_CUPS_GET_DEFAULT;
  ipp_message->request_id = 1;

  // Setup each attribute.
  IppAttributePtr attr_charset = BuildAttributePtr(
      "attributes-charset", IPP_TAG_OPERATION, IPP_TAG_CHARSET);
  attr_charset->value =
      ipp_parser::mojom::IppAttributeValue::NewStrings({"utf-8"});

  IppAttributePtr attr_natlang = BuildAttributePtr(
      "attributes-natural-language", IPP_TAG_OPERATION, IPP_TAG_LANGUAGE);
  attr_natlang->value =
      ipp_parser::mojom::IppAttributeValue::NewStrings({"en"});

  ipp_message->attributes.push_back(std::move(attr_charset));
  ipp_message->attributes.push_back(std::move(attr_natlang));
  ret->ipp = std::move(ipp_message);
  return ret;
}

// Ensure basic IPP request passes validation.
TEST_F(IppValidatorTest, SimpleSanityTest) {
  auto request = GetBasicIppRequest();
  auto validated_request = RunValidateIppRequest(request);
  EXPECT_TRUE(RunValidateIppRequest(request));
}

// Ensure printer endpoints are printers known to Chrome.
TEST_F(IppValidatorTest, EndpointIsKnownPrinter) {
  auto request = GetBasicIppRequest();
  std::string printer_id = "abc";

  request->endpoint = EncodeEndpointForPrinterId(printer_id);
  EXPECT_FALSE(RunValidateIppRequest(request));

  // Should succeed now that |delegate_| knows about the printer.
  delegate_->AddPrinter(printer_id);
  EXPECT_TRUE(RunValidateIppRequest(request));
}

TEST_F(IppValidatorTest, IncorrectHttpMethod) {
  auto request = GetBasicIppRequest();
  request->method = "GET";
  EXPECT_FALSE(RunValidateIppRequest(request));
}

TEST_F(IppValidatorTest, IncorrectHttpVersion) {
  auto request = GetBasicIppRequest();
  request->http_version = "HTTP/2.0";
  EXPECT_FALSE(RunValidateIppRequest(request));
}

TEST_F(IppValidatorTest, MissingHeaderName) {
  auto request = GetBasicIppRequest();

  // Adds new header with an empty name.
  request->headers[""] = "arbitrary valid header value";
  EXPECT_FALSE(RunValidateIppRequest(request));
}

TEST_F(IppValidatorTest, MissingHeaderValue) {
  auto request = GetBasicIppRequest();

  // Adds new header with an empty value.
  request->headers["arbitrary_valid_header_name"] = "";
  EXPECT_TRUE(RunValidateIppRequest(request));
}

// Test that we drop unknown attributes and succeed the request.
TEST_F(IppValidatorTest, UnknownAttribute) {
  auto request = GetBasicIppRequest();

  // Add fake attribute.
  std::string fake_attr_name = "fake-attribute-name";
  IppAttributePtr fake_attr =
      BuildAttributePtr(fake_attr_name, IPP_TAG_OPERATION, IPP_TAG_TEXT);
  fake_attr->value = ipp_parser::mojom::IppAttributeValue::NewStrings(
      {"fake_attribute_value"});
  request->ipp->attributes.push_back(std::move(fake_attr));

  auto result = RunValidateIppRequest(request);
  ASSERT_TRUE(result);

  // Ensure resulting validated IPP request doesn't contain fake_attr_name.
  ipp_t* ipp = result->ipp.get();
  EXPECT_FALSE(ippFindAttribute(ipp, fake_attr_name.c_str(), IPP_TAG_TEXT));
}

TEST_F(IppValidatorTest, IppDataVerification) {
  auto request = GetBasicIppRequest();

  // Should fail since the data too short to be a valid document.
  request->data = {0x0};
  EXPECT_FALSE(RunValidateIppRequest(request));

  // Valid PDF.
  request->data = {pdf_magic_bytes.begin(), pdf_magic_bytes.end()};
  EXPECT_TRUE(RunValidateIppRequest(request));

  // Valid PS.
  request->data = {ps_magic_bytes.begin(), ps_magic_bytes.end()};
  EXPECT_TRUE(RunValidateIppRequest(request));
}

// TODO(crbug.com/945409): Test IPP validation.

}  // namespace
}  // namespace cups_proxy
