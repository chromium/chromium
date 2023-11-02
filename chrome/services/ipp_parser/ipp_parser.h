// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_IPP_PARSER_IPP_PARSER_H_
#define CHROME_SERVICES_IPP_PARSER_IPP_PARSER_H_

#include <vector>

#include "chrome/services/ipp_parser/public/mojom/ipp_parser.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ipp_parser {

// ipp_parser.IppParser handler.
//
// This handler accepts incoming IPP requests as arbitrary buffers, parses
// the contents using libCUPS, and yields a chrome::mojom::IppRequest. It is
// intended to operate under the heavily jailed, out-of-process CupsIppParser
// Service.
class IppParser : public mojom::IppParser {
 public:
  explicit IppParser(mojo::PendingReceiver<mojom::IppParser> receiver);

  IppParser(const IppParser&) = delete;
  IppParser& operator=(const IppParser&) = delete;

  ~IppParser() override;

 private:
  // chrome::mojom::IppParser override.
  // Checks that |to_parse| is formatted as a valid IPP request, per RFC2910
  // Calls |callback| with a fully parsed IPP request on success, empty on
  // failure.
  void ParseIpp(const std::vector<uint8_t>& to_parse,
                ParseIppCallback callback) override;

  mojo::Receiver<mojom::IppParser> receiver_;
};

}  // namespace ipp_parser

#endif  // CHROME_SERVICES_IPP_PARSER_IPP_PARSER_H_
