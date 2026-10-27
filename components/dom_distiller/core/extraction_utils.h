// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_EXTRACTION_UTILS_H_
#define COMPONENTS_DOM_DISTILLER_CORE_EXTRACTION_UTILS_H_

#include <string>

#include "components/dom_distiller/core/proto/distilled_page.pb.h"

namespace base {
class Value;
}

namespace dom_distiller {

struct DistillerOptions;

// Keys for fields in the dictionary returned by the JavaScript distiller.
inline constexpr char kDistilledTitleKey[] = "title";
inline constexpr char kDistilledContentKey[] = "content";
inline constexpr char kDistilledTextDirectionKey[] = "dir";
inline constexpr char kDistilledTextContentKey[] = "textContent";

// Counts the number of words in the text_content portion.
int CountWords(const std::string& text_content);

// Parses the JS object returned by the web distiller into the
// DistilledPageProto expected by the distillation infra.
bool ParseDistillationResult(const base::Value& value,
                             DistilledPageProto* result);

// Returns the JavaScript web page distillation script with the given options.
std::string GetDistillerScript(const DistillerOptions& options);

// Returns the JavaScript heuristic to determine if web pages are suitable for
// reader mode.
std::string GetDistillerTriggeringScript();

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_EXTRACTION_UTILS_H_
