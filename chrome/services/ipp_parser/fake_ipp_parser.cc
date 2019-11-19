// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/ipp_parser/ipp_parser.h"

#include <memory>
#include <utility>

namespace ipp_parser {

IppParser::IppParser(mojo::PendingReceiver<mojom::IppParser> receiver)
    : receiver_(this, std::move(receiver)) {}

IppParser::~IppParser() = default;

void IppParser::ParseIpp(const std::vector<uint8_t>& to_parse,
                         ParseIppCallback callback) {
  DVLOG(1) << "IppParser stubbed";
  std::move(callback).Run(nullptr);
}

}  // namespace ipp_parser
