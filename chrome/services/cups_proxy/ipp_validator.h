// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_PROXY_IPP_VALIDATOR_H_
#define CHROME_SERVICES_CUPS_PROXY_IPP_VALIDATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "chrome/services/cups_proxy/cups_proxy_service_delegate.h"
#include "chrome/services/cups_proxy/public/cpp/ipp_messages.h"
#include "chrome/services/ipp_parser/public/cpp/ipp_converter.h"
#include "chrome/services/ipp_parser/public/mojom/ipp_parser.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cups_proxy {

struct IppRequest;

// This class fully validates incoming parsed IPP requests. HTTP metadata
// validation is handled with net/http. IPP metadata validation is handled
// largely via libCUPS. This class must be created and accessed from a
// sequenced context.
class IppValidator {
 public:
  explicit IppValidator(CupsProxyServiceDelegate* const delegate);
  ~IppValidator();

  // Validates each of |to_validate|'s fields and returns a POD representation
  // of the IPP request. Returns empty Optional on failure.
  absl::optional<IppRequest> ValidateIppRequest(
      ipp_parser::mojom::IppRequestPtr to_validate);

 private:
  absl::optional<HttpRequestLine> ValidateHttpRequestLine(
      base::StringPiece method,
      base::StringPiece endpoint,
      base::StringPiece http_version);

  absl::optional<std::vector<ipp_converter::HttpHeader>> ValidateHttpHeaders(
      const size_t http_content_length,
      const base::flat_map<std::string, std::string>& headers);

  ipp_t* ValidateIppMessage(ipp_parser::mojom::IppMessagePtr ipp_message);

  bool ValidateIppData(const std::vector<uint8_t>& ipp_data);

  // Unowned delegate providing necessary Profile dependencies.
  const raw_ptr<CupsProxyServiceDelegate, ExperimentalAsh> delegate_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace cups_proxy

#endif  // CHROME_SERVICES_CUPS_PROXY_IPP_VALIDATOR_H_
