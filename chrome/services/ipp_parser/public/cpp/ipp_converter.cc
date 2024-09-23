// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/ipp_parser/public/cpp/ipp_converter.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "net/http/http_util.h"

namespace ipp_converter {
namespace {

using IppAttributeValue = ipp_parser::mojom::IppAttributeValue;

const char kStatusDelimiter[] = " ";
const char kHeaderDelimiter[] = ": ";

const size_t kIppDateSize = 11;

// Callback used with ippReadIO (libCUPS API),
// Repeatedly used to copy IPP request buffer -> ipp_t.
ssize_t IppRead(base::span<const uint8_t>* src,
                ipp_uchar_t* dst,
                size_t bytes) {
  // Note: Cast here is safe since ipp_uchar_t == uint8_t and this will build
  // error if that ever changes.
  base::span<const ipp_uchar_t> safe_src(*src);

  size_t num_to_write = std::min(safe_src.size(), bytes);
  std::copy(safe_src.begin(), safe_src.begin() + num_to_write, dst);

  // Note: Modifying src here, not safe_src
  *src = src->subspan(num_to_write);

  return num_to_write;
}

// Callback used with ippWriteIO (libCUPS API),
// Repeatedly used to copy IPP ipp_t -> request buffer.
ssize_t IppWrite(base::span<uint8_t>* dst, ipp_uchar_t* source, size_t bytes) {
  // Note: Cast here is safe since ipp_uchar_t == uint8_t and this will build
  // error if that ever changes.
  uint8_t* src = static_cast<uint8_t*>(source);

  size_t num_to_write = std::min(dst->size(), bytes);
  std::copy(src, src + num_to_write, dst->begin());
  *dst = dst->subspan(num_to_write);

  return num_to_write;
}

// Returns a parsed HttpHeader on success, empty Optional on failure.
std::optional<HttpHeader> ParseHeader(std::string_view header) {
  if (base::Contains(header, kCarriage)) {
    return std::nullopt;
  }

  // Parse key
  const size_t key_end_index = header.find(":");
  if (key_end_index == std::string::npos || key_end_index == 0) {
    return std::nullopt;
  }

  const std::string_view key = header.substr(0, key_end_index);

  // Parse value
  const size_t value_begin_index = key_end_index + 1;
  if (value_begin_index == header.size()) {
    // Empty header value is valid
    return HttpHeader{std::string(key), ""};
  }

  std::string_view value = header.substr(value_begin_index);
  value = net::HttpUtil::TrimLWS(value);
  return HttpHeader{std::string(key), std::string(value)};
}

// Converts |value_tag| to corresponding mojom type for marshalling.
std::optional<IppAttributeValue::Tag> ValueTagToType(const int value_tag) {
  switch (value_tag) {
    case IPP_TAG_BOOLEAN:
      return IppAttributeValue::Tag::kBools;
    case IPP_TAG_DATE:
      return IppAttributeValue::Tag::kDate;
    case IPP_TAG_INTEGER:
    case IPP_TAG_ENUM:
      return IppAttributeValue::Tag::kInts;

    // Below string cases take from libCUPS ippAttributeString API
    case IPP_TAG_TEXT:
    case IPP_TAG_NAME:
    case IPP_TAG_KEYWORD:
    case IPP_TAG_CHARSET:
    case IPP_TAG_URI:
    case IPP_TAG_URISCHEME:
    case IPP_TAG_MIMETYPE:
    case IPP_TAG_LANGUAGE:
    case IPP_TAG_TEXTLANG:
    case IPP_TAG_NAMELANG:
      return IppAttributeValue::Tag::kStrings;

    // Octet (binary) string
    case IPP_TAG_STRING:
      return IppAttributeValue::Tag::kOctets;

    case IPP_TAG_RESOLUTION:
      return IppAttributeValue::Tag::kResolutions;

    default:
      break;
  }

  // Fail to convert any unrecognized types.
  DVLOG(1) << "Failed to convert CUPS value tag, type " << value_tag;
  return std::nullopt;
}

std::vector<bool> IppGetBools(ipp_attribute_t* attr) {
  const size_t count = ippGetCount(attr);

  std::vector<bool> ret;
  ret.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    // No decipherable failure condition for this libCUPS method.
    ret.push_back(ippGetBoolean(attr, i));
  }
  return ret;
}

std::optional<std::vector<int>> IppGetInts(ipp_attribute_t* attr) {
  const size_t count = ippGetCount(attr);

  std::vector<int> ret;
  ret.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    int v = ippGetInteger(attr, i);
    if (!v) {
      return std::nullopt;
    }
    ret.push_back(v);
  }
  return ret;
}

std::optional<std::vector<std::string>> IppGetStrings(ipp_attribute_t* attr) {
  const size_t count = ippGetCount(attr);

  std::vector<std::string> ret;
  ret.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    const char* v = ippGetString(
        attr, i, nullptr /* TODO(crbug.com/945409): figure out language */);
    if (!v) {
      return std::nullopt;
    }
    ret.emplace_back(v);
  }
  return ret;
}

std::optional<std::vector<std::vector<uint8_t>>> IppGetOctets(
    ipp_attribute_t* attr) {
  const size_t count = ippGetCount(attr);

  std::vector<std::vector<uint8_t>> ret;
  ret.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    int len = 0;
    const uint8_t* v =
        static_cast<const uint8_t*>(ippGetOctetString(attr, i, &len));
    if (!v || len <= 0) {
      return std::nullopt;
    }
    ret.emplace_back(v, v + len);
  }
  return ret;
}

std::optional<std::vector<ipp_parser::mojom::ResolutionPtr>> IppGetResolutions(
    ipp_attribute_t* attr) {
  const size_t count = ippGetCount(attr);

  std::vector<ipp_parser::mojom::ResolutionPtr> ret;
  ret.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    int xres = 0;
    int yres = 0;
    ipp_res_t units{};
    xres = ippGetResolution(attr, i, &yres, &units);
    if (xres <= 0 || yres <= 0 || units != IPP_RES_PER_INCH) {
      LOG(ERROR) << "bad resolution: " << xres << ", " << yres << ", "
                 << int(units);
      return std::nullopt;
    }
    ret.push_back(ipp_parser::mojom::Resolution(xres, yres).Clone());
  }
  return ret;
}

}  // namespace

std::optional<std::vector<std::string>> ParseRequestLine(
    std::string_view status_line) {
  // Split |status_slice| into triple method-endpoint-httpversion
  std::vector<std::string> terms =
      base::SplitString(status_line, kStatusDelimiter, base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_ALL);

  if (terms.size() != 3) {
    return std::nullopt;
  }

  return terms;
}

// Implicit conversion is safe since the conversion preserves memory layout.
std::optional<std::vector<uint8_t>> BuildRequestLine(
    std::string_view method,
    std::string_view endpoint,
    std::string_view http_version) {
  std::string status_line =
      base::StrCat({method, kStatusDelimiter, endpoint, kStatusDelimiter,
                    http_version, kCarriage});

  return std::vector<uint8_t>(status_line.begin(), status_line.end());
}

std::optional<std::vector<HttpHeader>> ParseHeaders(
    std::string_view headers_slice) {
  auto raw_headers = base::SplitStringPieceUsingSubstr(
      headers_slice, kCarriage, base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  std::vector<HttpHeader> ret;
  ret.reserve(raw_headers.size());
  for (auto raw_header : raw_headers) {
    auto header = ParseHeader(raw_header);
    if (!header) {
      return std::nullopt;
    }

    ret.push_back(header.value());
  }

  return ret;
}

std::optional<std::vector<uint8_t>> BuildHeaders(
    std::vector<HttpHeader> terms) {
  std::string headers;
  for (auto term : terms) {
    base::StrAppend(&headers,
                    {term.first, kHeaderDelimiter, term.second, kCarriage});
  }

  // End-of-headers sentinel is a double carriage return; add the second one.
  headers += kCarriage;

  return std::vector<uint8_t>(headers.begin(), headers.end());
}

// Synchronously reads/parses |ipp_slice| and returns the resulting ipp_t
// wrapper.
printing::ScopedIppPtr ParseIppMessage(base::span<const uint8_t> ipp_slice) {
  printing::ScopedIppPtr ipp = printing::WrapIpp(ippNew());

  // Casting IppRead callback to correct internal CUPS type
  // Note: This is safe since we essentially only cast the first argument from
  // base::span<const uint8_t> to void* and back, only accessing it from the
  // former.
  auto ret = ippReadIO(&ipp_slice, reinterpret_cast<ipp_iocb_t>(IppRead), 1,
                       nullptr, ipp.get());

  if (ret == IPP_STATE_ERROR) {
    // Read failed, clear and return nullptr
    ipp.reset();
  }

  return ipp;
}

std::optional<std::vector<uint8_t>> BuildIppMessage(ipp_t* ipp) {
  std::vector<uint8_t> request(ippLength(ipp));

  // Need to start in idle state for reading/writing.
  if (!ippSetState(ipp, IPP_STATE_IDLE)) {
    return std::nullopt;
  }

  // Casting IppWrite callback to correct internal CUPS type
  // Note: This is safe since we essentially only cast the first argument from
  // base::span<uint8_t> to void* and back, only accessing it from the former.
  base::span<uint8_t> request_view(request);
  auto ret = ippWriteIO(&request_view, reinterpret_cast<ipp_iocb_t>(IppWrite),
                        1, nullptr, ipp);

  if (ret == IPP_STATE_ERROR) {
    // Write failed
    return std::nullopt;
  }

  return request;
}

std::optional<std::vector<uint8_t>> BuildIppRequest(
    std::string_view method,
    std::string_view endpoint,
    std::string_view http_version,
    std::vector<HttpHeader> terms,
    ipp_t* ipp,
    std::vector<uint8_t> ipp_data) {
  // Build each subpart
  auto request_line_buffer = BuildRequestLine(method, endpoint, http_version);
  if (!request_line_buffer) {
    return std::nullopt;
  }

  auto headers_buffer = BuildHeaders(std::move(terms));
  if (!headers_buffer) {
    return std::nullopt;
  }

  auto ipp_message_buffer = BuildIppMessage(ipp);
  if (!ipp_message_buffer) {
    return std::nullopt;
  }

  // Marshall request
  std::vector<uint8_t> ret;
  const size_t request_size = request_line_buffer->size() +
                              headers_buffer->size() +
                              ipp_message_buffer->size() + ipp_data.size();
  ret.reserve(request_size);

  ret.insert(ret.end(), request_line_buffer->begin(),
             request_line_buffer->end());
  ret.insert(ret.end(), headers_buffer->begin(), headers_buffer->end());
  ret.insert(ret.end(), ipp_message_buffer->begin(), ipp_message_buffer->end());
  ret.insert(ret.end(), ipp_data.begin(), ipp_data.end());

  return ret;
}

// If no |ipp_data| is passed in, default to empty data portion.
std::optional<std::vector<uint8_t>> BuildIppRequest(
    std::string_view method,
    std::string_view endpoint,
    std::string_view http_version,
    std::vector<HttpHeader> terms,
    ipp_t* ipp) {
  return BuildIppRequest(method, endpoint, http_version, std::move(terms), ipp,
                         std::vector<uint8_t>());
}

// Parses and converts |ipp| to corresponding mojom type for marshalling.
// Returns nullptr on failure.
ipp_parser::mojom::IppMessagePtr ConvertIppToMojo(ipp_t* ipp) {
  ipp_parser::mojom::IppMessagePtr ret = ipp_parser::mojom::IppMessage::New();

  // Parse version numbers
  int major, minor;
  major = ippGetVersion(ipp, &minor);
  ret->major_version = major;
  ret->minor_version = minor;

  // IPP request opcode ids are specified by the RFC, so casting to ints is
  // safe.
  ret->operation_id = static_cast<int>(ippGetOperation(ipp));
  if (!ret->operation_id) {
    return nullptr;
  }

  // Parse request id
  ret->request_id = ippGetRequestId(ipp);
  if (!ret->request_id) {
    return nullptr;
  }

  std::vector<ipp_parser::mojom::IppAttributePtr> attributes;
  for (ipp_attribute_t* attr = ippFirstAttribute(ipp); attr != nullptr;
       attr = ippNextAttribute(ipp)) {
    ipp_parser::mojom::IppAttributePtr attrptr =
        ipp_parser::mojom::IppAttribute::New();

    auto* name = ippGetName(attr);
    if (!name) {
      return nullptr;
    }
    attrptr->name = name;

    attrptr->group_tag = ippGetGroupTag(attr);
    if (attrptr->group_tag == IPP_TAG_ZERO) {
      return nullptr;
    }

    attrptr->value_tag = ippGetValueTag(attr);
    if (attrptr->value_tag == IPP_TAG_ZERO) {
      return nullptr;
    }

    auto type = ValueTagToType(attrptr->value_tag);
    if (!type) {
      return nullptr;
    }

    switch (*type) {
      case IppAttributeValue::Tag::kBools: {
        attrptr->value =
            ipp_parser::mojom::IppAttributeValue::NewBools(IppGetBools(attr));
        break;
      }
      case IppAttributeValue::Tag::kDate: {
        // Note: We expect date-attributes to be single-valued.
        const uint8_t* v =
            reinterpret_cast<const uint8_t*>(ippGetDate(attr, 0));
        if (!v) {
          return nullptr;
        }
        attrptr->value = ipp_parser::mojom::IppAttributeValue::NewDate(
            std::vector<uint8_t>(v, v + kIppDateSize));
        break;
      }
      case IppAttributeValue::Tag::kInts: {
        auto vals = IppGetInts(attr);
        if (!vals.has_value()) {
          return nullptr;
        }
        attrptr->value = ipp_parser::mojom::IppAttributeValue::NewInts(*vals);
        break;
      }
      case IppAttributeValue::Tag::kStrings: {
        auto vals = IppGetStrings(attr);
        if (!vals.has_value()) {
          return nullptr;
        }
        attrptr->value =
            ipp_parser::mojom::IppAttributeValue::NewStrings(*vals);
        break;
      }
      case IppAttributeValue::Tag::kOctets: {
        auto vals = IppGetOctets(attr);
        if (!vals.has_value()) {
          return nullptr;
        }
        attrptr->value = ipp_parser::mojom::IppAttributeValue::NewOctets(*vals);
        break;
      }
      case IppAttributeValue::Tag::kResolutions: {
        auto vals = IppGetResolutions(attr);
        if (!vals.has_value()) {
          return nullptr;
        }
        attrptr->value = ipp_parser::mojom::IppAttributeValue::NewResolutions(
            std::move(*vals));
        break;
      }
    }

    if (!attrptr->value)
      NOTREACHED_IN_MIGRATION();

    attributes.push_back(std::move(attrptr));
  }

  ret->attributes = std::move(attributes);
  return ret;
}

}  // namespace ipp_converter
