// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Stolen from chrome/browser/component_updater/component_unpacker.cc

#include "chrome/chrome_cleaner/components/component_unpacker.h"

#include <memory>
#include <vector>

#include "components/crx_file/crx_verifier.h"
#include "third_party/zlib/google/zip.h"

namespace chrome_cleaner {

ComponentUnpacker::ComponentUnpacker(const std::vector<uint8_t>& pk_hash,
                                     const base::FilePath& path)
    : pk_hash_(pk_hash), path_(path) {}

bool ComponentUnpacker::Unpack(const base::FilePath& ouput_folder) {
  return Verify() && zip::Unzip(path_, ouput_folder);
}

bool ComponentUnpacker::Verify() {
  return crx_file::Verify(path_, crx_file::VerifierFormat::CRX3, {pk_hash_},
                          std::vector<uint8_t>(), nullptr,
                          nullptr) == crx_file::VerifierResult::OK_FULL;
}

ComponentUnpacker::~ComponentUnpacker() {}

}  // namespace chrome_cleaner
