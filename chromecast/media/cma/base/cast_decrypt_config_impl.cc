// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/cast_decrypt_config_impl.h"

namespace chromecast {
namespace media {

CastDecryptConfigImpl::CastDecryptConfigImpl(
    std::string key_id,
    std::string iv,
    const EncryptionPattern& pattern,
    std::vector<SubsampleEntry> subsamples)
    : key_id_(std::move(key_id)),
      iv_(std::move(iv)),
      pattern_(pattern),
      subsamples_(std::move(subsamples)) {}

CastDecryptConfigImpl::~CastDecryptConfigImpl() {}

const std::string& CastDecryptConfigImpl::key_id() const {
  return key_id_;
}

const std::string& CastDecryptConfigImpl::iv() const {
  return iv_;
}

const EncryptionPattern& CastDecryptConfigImpl::pattern() const {
  return pattern_;
}

const std::vector<SubsampleEntry>& CastDecryptConfigImpl::subsamples() const {
  return subsamples_;
}

}  // namespace media
}  // namespace chromecast
