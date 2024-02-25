// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SCREEN_AI_PROTO_MAIN_CONTENT_EXTRACTOR_PROTO_CONVERTOR_H_
#define COMPONENTS_SERVICES_SCREEN_AI_PROTO_MAIN_CONTENT_EXTRACTOR_PROTO_CONVERTOR_H_

#include <map>
#include <string>

#include "ui/accessibility/ax_tree_update.h"

namespace screen_ai {

// Converts an AXTreeUpdate snapshot to serialized ViewHierarchy proto for
// MainContentExtractor.
std::string SnapshotToViewHierarchy(const ui::AXTreeUpdate& snapshot);

// Returns a map of MainContentExtractor role strings to Chrome roles.
std::map<std::string, ax::mojom::Role>
GetMainContentExtractorToChromeRoleConversionMapForTesting();

}  // namespace screen_ai

#endif  // COMPONENTS_SERVICES_SCREEN_AI_PROTO_MAIN_CONTENT_EXTRACTOR_PROTO_CONVERTOR_H_
