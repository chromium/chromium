// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_INTEGRITY_BLOCK_ATTRIBUTES_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_INTEGRITY_BLOCK_ATTRIBUTES_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "mojo/public/cpp/bindings/default_construct_tag.h"

namespace web_package {

class IntegrityBlockAttributes {
 public:
  IntegrityBlockAttributes(std::string web_bundle_id,
                           std::vector<uint8_t> cbor);
  ~IntegrityBlockAttributes();

  bool operator<=>(const IntegrityBlockAttributes& other) const = default;

  IntegrityBlockAttributes(const IntegrityBlockAttributes&);
  IntegrityBlockAttributes& operator=(const IntegrityBlockAttributes&);

  const std::string& web_bundle_id() const { return web_bundle_id_; }
  const std::vector<uint8_t>& cbor() const { return cbor_; }

  explicit IntegrityBlockAttributes(mojo::DefaultConstruct::Tag);

 private:
  std::string web_bundle_id_;
  std::vector<uint8_t> cbor_;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_INTEGRITY_BLOCK_ATTRIBUTES_H_
