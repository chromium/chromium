// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_CONTEXT_ID_H_
#define COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_CONTEXT_ID_H_

#include <string>

#include "base/types/strong_alias.h"

namespace data_sharing {

// A stable ID that provides a reliable key to link private and shared data
// sets.
struct ContextId : public base::StrongAlias<class ContextIdTag, std::string> {
  using base::StrongAlias<class ContextIdTag, std::string>::StrongAlias;

  // Converts the ContextId to a string.
  const std::string& AsString() const { return value(); }

  // Creates a ContextId from a string.
  static ContextId FromString(const std::string& str) { return ContextId(str); }
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_CONTEXT_ID_H_
