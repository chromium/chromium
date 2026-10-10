// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/certificate_util.h"

#include "third_party/boringssl/src/pki/pem.h"

namespace private_ai {

namespace {

constexpr std::string_view kAllowedBlockTypes[] = {"CERTIFICATE"};

}  // namespace

std::optional<std::string> PemToDer(std::string_view pem_content) {
  bssl::PEMTokenizer tokenizer(pem_content, kAllowedBlockTypes);
  if (!tokenizer.GetNext() || tokenizer.data().empty()) {
    return std::nullopt;
  }

  return tokenizer.data();
}

std::vector<std::string> PemToDerChain(std::string_view pem_content) {
  std::vector<std::string> chain;
  bssl::PEMTokenizer tokenizer(pem_content, kAllowedBlockTypes);
  while (tokenizer.GetNext()) {
    if (!tokenizer.data().empty()) {
      chain.push_back(tokenizer.data());
    }
  }
  return chain;
}

}  // namespace private_ai
