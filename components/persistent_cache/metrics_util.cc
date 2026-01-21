// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/metrics_util.h"

#include "base/strings/strcat.h"
#include "components/persistent_cache/client.h"

namespace persistent_cache {

namespace {

std::string_view ClientToTag(Client client) {
  switch (client) {
    case Client::kCodeCache:
      return "CodeCache";
    case Client::kShaderCache:
      return "ShaderCache";
    case Client::kTest:
      return "Test";
  }
}

}  // namespace

std::string GetHistogramName(Client client,
                             std::string_view metric,
                             std::optional<bool> is_read_write) {
  return base::StrCat({"PersistentCache.", metric,
                       is_read_write.has_value()
                           ? (*is_read_write ? std::string_view(".ReadWrite.")
                                             : std::string_view(".ReadOnly."))
                           : std::string_view("."),
                       ClientToTag(client)});
}

}  // namespace persistent_cache
