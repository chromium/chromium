// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/transcript_builder.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash::babelorca {

TranscriptBuilder::Result::Result(const std::string& text_param,
                                  bool is_final_param,
                                  const std::string& language_param)
    : text(text_param), is_final(is_final_param), language(language_param) {}

TranscriptBuilder::TranscriptBuilder(const std::string& session_id,
                                     const std::string& sender_email)
    : session_id_(session_id), sender_email_(sender_email) {}

TranscriptBuilder::~TranscriptBuilder() = default;

std::vector<TranscriptBuilder::Result> TranscriptBuilder::GetTranscripts(
    mojom::BabelOrcaMessagePtr message) {
  // Discard message if not a valid session, not a valid sender, or it is an old
  // message.
  if (message->session_id != session_id_ ||
      !gaia::AreEmailsSame(sender_email_, message->sender_email.value_or("")) ||
      message->init_timestamp_ms < init_timestamp_ms_ ||
      (message->init_timestamp_ms == init_timestamp_ms_ &&
       message->order <= order_)) {
    return {};
  }

  order_ = message->order;
  std::vector<Result> results;
  // Discard previously received transcripts if the sender is a newer
  // sender instance.
  if (message->init_timestamp_ms > init_timestamp_ms_) {
    if (init_timestamp_ms_ > -1 && !is_final_) {
      results.emplace_back(text_, /*is_final=*/true, language_);
    }
    results.emplace_back(message->current_transcript->text,
                         message->current_transcript->is_final,
                         message->current_transcript->language);
    init_timestamp_ms_ = message->init_timestamp_ms;
    Update(std::move(message->current_transcript));
    return results;
  }

  if (!message->previous_transcript.is_null() && !is_final_ &&
      message->previous_transcript->transcript_id == transcript_id_) {
    results = MaybeMergeTranscript(std::move(message->previous_transcript),
                                   /*is_previous=*/true);
  }
  std::vector<Result> current_results =
      MaybeMergeTranscript(std::move(message->current_transcript),
                           /*is_previous=*/false);
  for (auto& result : current_results) {
    results.push_back(std::move(result));
  }
  Update(std::move(message->current_transcript));
  return results;
}

std::vector<TranscriptBuilder::Result> TranscriptBuilder::MaybeMergeTranscript(
    const mojom::TranscriptPartPtr& transcript_part,
    bool is_previous) {
  std::vector<Result> results;
  if (transcript_part->transcript_id == transcript_id_ && !is_final_) {
    int64_t previous_size = text_index_ + static_cast<int64_t>(text_.size());
    if (transcript_part->text_index > text_index_ &&
        transcript_part->text_index <= previous_size) {
      std::string new_text = base::StrCat(
          {text_.substr(0, transcript_part->text_index - text_index_),
           transcript_part->text});
      results.emplace_back(new_text, transcript_part->is_final,
                           transcript_part->language);
      if (!is_previous) {
        is_final_ = transcript_part->is_final;
        text_ = std::move(new_text);
      }
      return results;
    }

    // If there is a gap between the previous transcript part built and the new
    // part, commit the previously part built and start building a new one.
    if (transcript_part->text_index > previous_size) {
      results.emplace_back(text_, /*is_final=*/true, transcript_part->language);
    }
  }
  results.emplace_back(transcript_part->text, transcript_part->is_final,
                       transcript_part->language);
  return results;
}

void TranscriptBuilder::Update(mojom::TranscriptPartPtr transcript_part) {
  transcript_id_ = transcript_part->transcript_id;
  language_ = std::move(transcript_part->language);
  text_ = std::move(transcript_part->text);
  text_index_ = transcript_part->text_index;
  is_final_ = transcript_part->is_final;
}

}  // namespace ash::babelorca
