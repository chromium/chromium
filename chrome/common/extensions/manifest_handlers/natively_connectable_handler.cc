// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/natively_connectable_handler.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace {

const NativelyConnectableHosts* GetHosts(const Extension& extension) {
  return static_cast<const NativelyConnectableHosts*>(
      extension.GetManifestData(manifest_keys::kNativelyConnectable));
}

}  // namespace

NativelyConnectableHosts::NativelyConnectableHosts() = default;
NativelyConnectableHosts::~NativelyConnectableHosts() = default;

// static
const std::set<std::string>*
NativelyConnectableHosts::GetConnectableNativeMessageHosts(
    const Extension& extension) {
  const auto* hosts = GetHosts(extension);
  if (!hosts) {
    return nullptr;
  }
  return &hosts->hosts;
}

NativelyConnectableHandler::NativelyConnectableHandler() = default;
NativelyConnectableHandler::~NativelyConnectableHandler() = default;

bool NativelyConnectableHandler::Parse(Extension* extension,
                                       base::string16* error) {
  const base::Value* natively_connectable_hosts = nullptr;
  if (!extension->manifest()->GetList(manifest_keys::kNativelyConnectable,
                                      &natively_connectable_hosts)) {
    *error = base::ASCIIToUTF16(manifest_errors::kInvalidNativelyConnectable);
    return false;
  }

  auto hosts = std::make_unique<NativelyConnectableHosts>();
  for (const auto& host : natively_connectable_hosts->GetList()) {
    if (!host.is_string() || host.GetString().empty()) {
      *error =
          base::ASCIIToUTF16(manifest_errors::kInvalidNativelyConnectableValue);
      return false;
    }
    hosts->hosts.insert(host.GetString());
  }

  extension->SetManifestData(manifest_keys::kNativelyConnectable,
                             std::move(hosts));
  return true;
}

base::span<const char* const> NativelyConnectableHandler::Keys() const {
  static constexpr const char* kKeys[] = {manifest_keys::kNativelyConnectable};
  return kKeys;
}

}  // namespace extensions
