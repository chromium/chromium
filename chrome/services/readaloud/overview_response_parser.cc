// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/overview_response_parser.h"

#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/readaloud/read_aloud_constants.h"
#include "components/optimization_guide/proto/features/read_aloud_generate_text.pb.h"

namespace readaloud {

namespace {

// DialogueTurn.speaker value of the primary persona. By contract,
// (read_aloud_ai_playback_prompts.gcl) this name is always "Host";
// the secondary persona is named by role (e.g. "Guest", "Expert", "Reporter").
constexpr char kHostRole[] = "Host";


// The "Host" persona (or an unset role) is the primary voice; any other
// persona (e.g. "Reporter") is the secondary voice.
read_aloud::mojom::Speaker SpeakerForRole(std::string_view role) {
  return role.empty() || base::EqualsCaseInsensitiveASCII(role, kHostRole)
             ? read_aloud::mojom::Speaker::kSpeaker1
             : read_aloud::mojom::Speaker::kSpeaker2;
}

// Appends a segment for `text`. Returns false if a `SetTextContent()` limit
// would be exceeded. These bounds (kMaxTextSegments, kMaxTextLengthPerSegment,
// kMaxMojoPayloadSizeBytes) provide roughly 100x headroom over expected MES
// GenerateText output (about 300 words across 10-30 dialogue turns) while
// guaranteeing memory safety and Mojo buffer conformance.
bool AppendSegment(std::string_view text,
                   read_aloud::mojom::Speaker speaker,
                   size_t& total_text_bytes,
                   std::vector<read_aloud::mojom::TextSegmentPtr>& segments) {
  if (segments.size() >= kMaxTextSegments) {
    return false;
  }
  auto segment = read_aloud::mojom::TextSegment::New();
  segment->segment_index = static_cast<uint32_t>(segments.size());
  segment->text = base::UTF8ToUTF16(text);
  segment->speaker = speaker;
  if (segment->text.size() > kMaxTextLengthPerSegment) {
    return false;
  }
  total_text_bytes += segment->text.size() * sizeof(char16_t);
  if (total_text_bytes > kMaxMojoPayloadSizeBytes) {
    return false;
  }
  segments.push_back(std::move(segment));
  return true;
}

}  // namespace

ParsedOverviewResult::ParsedOverviewResult() = default;
ParsedOverviewResult::ParsedOverviewResult(ParsedOverviewResult&&) = default;
ParsedOverviewResult& ParsedOverviewResult::operator=(ParsedOverviewResult&&) =
    default;
ParsedOverviewResult::~ParsedOverviewResult() = default;

ParsedOverviewResult ParseAndValidateOverviewResponse(
    mojo_base::BigBuffer response_bytes) {
  ParsedOverviewResult result;

  if (response_bytes.size() == 0 ||
      response_bytes.size() > kMaxMojoPayloadSizeBytes) {
    result.status = OverviewParseStatus::kMalformed;
    return result;
  }

  optimization_guide::proto::ReadAloudGenerateTextResponse response;
  if (!response.ParseFromArray(response_bytes.data(), response_bytes.size())) {
    result.status = OverviewParseStatus::kMalformed;
    return result;
  }

  size_t total_text_bytes = 0;
  for (int i = 0; i < response.dialogue_turns_size(); ++i) {
    const auto& turn = response.dialogue_turns(i);
    std::string trimmed_utterance;
    base::TrimWhitespaceASCII(turn.utterance(), base::TRIM_ALL,
                              &trimmed_utterance);
    if (trimmed_utterance.empty()) {
      continue;
    }
    read_aloud::mojom::Speaker speaker = SpeakerForRole(turn.speaker());
    if (!AppendSegment(trimmed_utterance, speaker, total_text_bytes,
                       result.segments)) {
      result.segments.clear();
      result.status = OverviewParseStatus::kMalformed;
      return result;
    }
  }

  if (result.segments.empty()) {
    result.status = OverviewParseStatus::kEmpty;
    return result;
  }

  if (response.title().size() <= kMaxOverviewMetadataLength) {
    result.title = response.title();
  } else {
    result.title = std::string(
        base::TruncateUTF8ToByteSize(response.title(), kMaxOverviewMetadataLength));
  }
  result.status = OverviewParseStatus::kOk;
  return result;
}

}  // namespace readaloud
