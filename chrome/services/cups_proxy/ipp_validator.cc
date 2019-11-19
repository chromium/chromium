// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/ipp_validator.h"

#include <cups/cups.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "chrome/services/cups_proxy/ipp_attribute_validator.h"
#include "chrome/services/cups_proxy/public/cpp/cups_util.h"
#include "net/http/http_util.h"
#include "printing/backend/cups_ipp_util.h"

namespace cups_proxy {
namespace {

using ValueType = ipp_parser::mojom::ValueType;

// Initial version only supports english lcoales.
// TODO(crbug.com/945409): Extending to supporting arbitrary locales.
const char kLocaleEnglish[] = "en";

// Converting to vector<char> for libCUPS API:
// ippAddBooleans(..., int num_values, const char *values)
std::vector<char> ConvertBooleans(std::vector<bool> bools) {
  std::vector<char> ret;
  for (bool value : bools) {
    ret.push_back(value ? 1 : 0);
  }
  return ret;
}

// Converting to vector<const char*> for libCUPS API:
// ippAddStrings(..., int num_values, const char *const *values)
// Note: The values in the returned vector refer to |strings|; so |strings|
// must outlive them.
std::vector<const char*> ConvertStrings(
    const std::vector<std::string>& strings) {
  std::vector<const char*> ret;
  for (auto& value : strings) {
    ret.push_back(value.c_str());
  }
  return ret;
}

// Depending on |type|, returns the number of values associated with |attr|.
size_t GetAttributeValuesSize(const ipp_parser::mojom::IppAttributePtr& attr) {
  const auto& attr_value = attr->value;
  switch (attr->type) {
    case ValueType::DATE:
      return 1;

    case ValueType::BOOLEAN:
      DCHECK(attr_value->is_bools());
      return attr_value->get_bools().size();
    case ValueType::INTEGER:
      DCHECK(attr_value->is_ints());
      return attr_value->get_ints().size();
    case ValueType::STRING:
      DCHECK(attr_value->is_strings());
      return attr_value->get_strings().size();

    default:
      break;
  }

  DVLOG(1) << "Unknown CupsIppParser ValueType " << attr->type;
  return 0;
}

// Returns true if |data| starts with the full |prefix|, false otherwise.
bool StartsWith(base::span<uint8_t const> data,
                base::span<uint8_t const> prefix) {
  if (data.size() < prefix.size())
    return false;
  return std::equal(data.begin(), data.begin() + prefix.size(), prefix.begin());
}

}  // namespace

// Verifies that |method|, |endpoint|, and |http_version| form a valid HTTP
// request-line. On success, returns a wrapper obj containing the verified
// request-line.
base::Optional<HttpRequestLine> IppValidator::ValidateHttpRequestLine(
    base::StringPiece method,
    base::StringPiece endpoint,
    base::StringPiece http_version) {
  if (method != "POST") {
    return base::nullopt;
  }
  if (http_version != "HTTP/1.1") {
    return base::nullopt;
  }

  // Empty endpoint is allowed.
  if (endpoint == "/") {
    return HttpRequestLine{method.as_string(), endpoint.as_string(),
                           http_version.as_string()};
  }

  // Ensure endpoint is a known printer.
  auto printer_id = ParseEndpointForPrinterId(endpoint.as_string());
  if (!printer_id.has_value()) {
    return base::nullopt;
  }

  auto printer = delegate_->GetPrinter(*printer_id);
  if (!printer.has_value()) {
    return base::nullopt;
  }

  return HttpRequestLine{method.as_string(), endpoint.as_string(),
                         http_version.as_string()};
}

base::Optional<std::vector<ipp_converter::HttpHeader>>
IppValidator::ValidateHttpHeaders(
    const size_t http_content_length,
    const base::flat_map<std::string, std::string>& headers) {
  // Sane, character-set checks.
  for (const auto& header : headers) {
    if (!net::HttpUtil::IsValidHeaderName(header.first) ||
        !net::HttpUtil::IsValidHeaderValue(header.second)) {
      return base::nullopt;
    }
  }

  std::vector<ipp_converter::HttpHeader> ret(headers.begin(), headers.end());

  // Update the ContentLength.
  base::EraseIf(ret, [](const ipp_converter::HttpHeader& header) {
    return header.first == "Content-Length";
  });
  ret.push_back({"Content-Length", base::NumberToString(http_content_length)});

  return ret;
}

// Note: Since its possible to have valid IPP attributes that our
// ipp_attribute_validator.cc is unaware of, we drop unknown attributes, rather
// than fail the request.
ipp_t* IppValidator::ValidateIppMessage(
    ipp_parser::mojom::IppMessagePtr ipp_message) {
  printing::ScopedIppPtr ipp = printing::WrapIpp(ippNew());

  // Fill ids.
  if (!ippSetVersion(ipp.get(), ipp_message->major_version,
                     ipp_message->minor_version)) {
    return nullptr;
  }

  const ipp_op_t ipp_oper_id = static_cast<ipp_op_t>(ipp_message->operation_id);
  if (!ippSetOperation(ipp.get(), ipp_oper_id))
    return nullptr;

  if (!ippSetRequestId(ipp.get(), ipp_message->request_id)) {
    return nullptr;
  }

  // Fill attributes.
  for (size_t i = 0; i < ipp_message->attributes.size(); ++i) {
    ipp_parser::mojom::IppAttributePtr attribute =
        std::move(ipp_message->attributes[i]);

    size_t num_values = GetAttributeValuesSize(attribute);
    if (!num_values) {
      return nullptr;
    }

    auto ret = ValidateAttribute(ipp_oper_id, attribute->name, attribute->type,
                                 num_values);
    if (ret == ValidateAttributeResult::kFatalError) {
      return nullptr;
    }
    if (ret == ValidateAttributeResult::kUnknownAttribute) {
      // We drop unknown attributes.
      DVLOG(1) << "CupsProxy validation: dropping unknown attribute "
               << attribute->name;
      continue;
    }

    switch (attribute->type) {
      case ValueType::BOOLEAN: {
        DCHECK(attribute->value->is_bools());
        std::vector<char> values =
            ConvertBooleans(attribute->value->get_bools());

        auto* attr = ippAddBooleans(
            ipp.get(), static_cast<ipp_tag_t>(attribute->group_tag),
            attribute->name.c_str(), values.size(), values.data());
        if (!attr) {
          return nullptr;
        }
        break;
      }
      case ValueType::DATE: {
        DCHECK(attribute->value->is_date());
        std::vector<uint8_t> date = attribute->value->get_date();

        // Per RFC2910, ipp_uchar_t is defined as an OCTET, so below
        // reinterpret_cast is safe.
        auto* attr =
            ippAddDate(ipp.get(), static_cast<ipp_tag_t>(attribute->group_tag),
                       attribute->name.c_str(),
                       reinterpret_cast<const ipp_uchar_t*>(date.data()));
        if (!attr) {
          return nullptr;
        }
        break;
      }
      case ValueType::INTEGER: {
        DCHECK(attribute->value->is_ints());
        std::vector<int> values = attribute->value->get_ints();

        auto* attr = ippAddIntegers(
            ipp.get(), static_cast<ipp_tag_t>(attribute->group_tag),
            static_cast<ipp_tag_t>(attribute->value_tag),
            attribute->name.c_str(), values.size(), values.data());
        if (!attr) {
          return nullptr;
        }
        break;
      }
      case ValueType::STRING: {
        DCHECK(attribute->value->is_strings());

        // Note: cstrings_values references attribute->value's strings, i.e.
        // attribute->value MUST outlive cstrings_values.
        std::vector<const char*> cstrings_values =
            ConvertStrings(attribute->value->get_strings());

        auto* attr = ippAddStrings(
            ipp.get(), static_cast<ipp_tag_t>(attribute->group_tag),
            static_cast<ipp_tag_t>(attribute->value_tag),
            attribute->name.c_str(), cstrings_values.size(), kLocaleEnglish,
            cstrings_values.data());
        if (!attr) {
          return nullptr;
        }
        break;
      }
      default:
        NOTREACHED() << "Unknown IPP attribute type found.";
    }
  }

  // Validate Attributes.
  if (!ippValidateAttributes(ipp.get())) {
    return nullptr;
  }

  // Return built ipp object.
  return ipp.release();
}

// Requires |ipp_data| to be empty or look like a PDF or PostScript document.
bool IppValidator::ValidateIppData(const std::vector<uint8_t>& ipp_data) {
  // Empty IPP data portion.
  if (ipp_data.empty()) {
    return true;
  }

  // Check if |ipp_data| looks like a PDF or PostScript document.
  return StartsWith(ipp_data, pdf_magic_bytes) ||
         StartsWith(ipp_data, ps_magic_bytes);
}

IppValidator::IppValidator(CupsProxyServiceDelegate* const delegate)
    : delegate_(delegate) {}

IppValidator::~IppValidator() = default;

base::Optional<IppRequest> IppValidator::ValidateIppRequest(
    ipp_parser::mojom::IppRequestPtr to_validate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Build ipp message.
  // Note: Moving ipp here, to_validate->ipp no longer valid below.
  printing::ScopedIppPtr ipp =
      printing::WrapIpp(ValidateIppMessage(std::move(to_validate->ipp)));
  if (ipp == nullptr) {
    return base::nullopt;
  }

  // Validate ipp data.
  // TODO(crbug/894607): Validate ippData (pdf).
  if (!ValidateIppData(to_validate->data)) {
    return base::nullopt;
  }

  // Build request line.
  auto request_line = ValidateHttpRequestLine(
      to_validate->method, to_validate->endpoint, to_validate->http_version);
  if (!request_line.has_value()) {
    return base::nullopt;
  }

  // Build headers; must happen after ipp message/data since it requires the
  // ContentLength.
  const size_t http_content_length =
      ippLength(ipp.get()) + to_validate->data.size();
  auto headers = ValidateHttpHeaders(http_content_length, to_validate->headers);
  if (!headers.has_value()) {
    return base::nullopt;
  }

  // Marshall request
  IppRequest ret;
  ret.request_line = std::move(*request_line);
  ret.headers = std::move(*headers);
  ret.ipp = std::move(ipp);
  ret.ipp_data = std::move(to_validate->data);

  // Build parsed request buffer.
  auto request_buffer = ipp_converter::BuildIppRequest(
      ret.request_line.method, ret.request_line.endpoint,
      ret.request_line.http_version, ret.headers, ret.ipp.get(), ret.ipp_data);
  if (!request_buffer.has_value()) {
    return base::nullopt;
  }

  ret.buffer = std::move(*request_buffer);
  return ret;
}

}  // namespace cups_proxy
