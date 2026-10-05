// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_OVERVIEW_RESPONSE_PARSER_H_
#define CHROME_SERVICES_READALOUD_OVERVIEW_RESPONSE_PARSER_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "chrome/common/readaloud/read_aloud.mojom.h"
#include "mojo/public/cpp/base/big_buffer.h"

namespace readaloud {

enum class OverviewParseStatus {
  // The response was parsed into a non-empty script.
  kOk,
  // The payload failed to deserialize or exceeded size limits.
  kMalformed,
  // The response contained no speakable text.
  kEmpty,
};

struct ParsedOverviewResult {
  ParsedOverviewResult();
  ParsedOverviewResult(ParsedOverviewResult&&);
  ParsedOverviewResult& operator=(ParsedOverviewResult&&);
  ~ParsedOverviewResult();

  OverviewParseStatus status = OverviewParseStatus::kMalformed;
  // Sanitized title metadata for the UI (guaranteed valid UTF-8, invalid
  // bytes replaced with U+FFFD, and truncated to length cap). Set only on kOk.
  std::string title;
  // Satisfies the SetTextContent() limits. Set only on kOk.
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
};

// Deserializes and validates a serialized ReadAloudGenerateTextResponse in the
// sandboxed utility process.
ParsedOverviewResult ParseAndValidateOverviewResponse(
    mojo_base::BigBuffer response_bytes);

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_OVERVIEW_RESPONSE_PARSER_H_
