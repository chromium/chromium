// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/common/pnacl_types.h"

namespace nacl {

PnaclCacheInfo::PnaclCacheInfo()
    : abi_version(0),
      opt_level(0),
      has_no_store_header(false),
      use_subzero(false) {}
PnaclCacheInfo::PnaclCacheInfo(const PnaclCacheInfo& other) = default;
PnaclCacheInfo::~PnaclCacheInfo() = default;

}  // namespace nacl
