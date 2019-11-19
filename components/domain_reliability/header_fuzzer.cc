// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "components/domain_reliability/header.h"

using domain_reliability::DomainReliabilityHeader;

// The URL parser depends on ICU, which must be initialized.
struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // ICU requires an AtExitManager.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment* env = new IcuEnvironment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::StringPiece input(reinterpret_cast<const char*>(data), size);
  std::unique_ptr<DomainReliabilityHeader> header =
      DomainReliabilityHeader::Parse(input);
  if (header->IsSetConfig()) {
    header->ToString();
  }
  return 0;
}
