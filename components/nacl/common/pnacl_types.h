// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_COMMON_PNACL_TYPES_H_
#define COMPONENTS_NACL_COMMON_PNACL_TYPES_H_

// This file exists (instead of putting this type into nacl_types.h) because
// nacl_types is built into nacl_helper in addition to chrome, and we don't
// want to pull src/url/ into there, since it would be unnecessary bloat.

#include "base/time/time.h"
#include "url/gurl.h"

namespace nacl {
// Cache-related information about pexe files, sent from the plugin/renderer
// to the browser.
//
// If you change this, you will also need to update the IPC serialization in
// nacl_host_messages.h.
struct PnaclCacheInfo {
  PnaclCacheInfo();
  PnaclCacheInfo(const PnaclCacheInfo& other);
  ~PnaclCacheInfo();
  GURL pexe_url;
  int abi_version;
  int opt_level;
  base::Time last_modified;
  std::string etag;
  bool has_no_store_header;
  bool use_subzero;
  std::string sandbox_isa;
  std::string extra_flags;
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_COMMON_PNACL_TYPES_H_
