// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSCRIPT_BUILDER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSCRIPT_BUILDER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom-forward.h"

namespace ash::babelorca {

// Class to incrementally build transcripts as parts of them are received.
class TranscriptBuilder {
 public:
  struct Result {
    Result(const std::string& text_param,
           bool is_final_param,
           const std::string& language_param);
    bool operator==(const Result& other) const = default;
    std::string text;
    bool is_final;
    std::string language;
  };
  TranscriptBuilder(const std::string& session_id,
                    const std::string& sender_email);

  TranscriptBuilder(const TranscriptBuilder&) = delete;
  TranscriptBuilder& operator=(const TranscriptBuilder&) = delete;

  ~TranscriptBuilder();

  // Merges the newly received `message` with the already received ones and
  // returns a vector of the updated built transcripts.
  std::vector<Result> GetTranscripts(mojom::BabelOrcaMessagePtr message);

 private:
  std::vector<Result> MaybeMergeTranscript(
      const mojom::TranscriptPartPtr& transcript_part,
      bool is_previous);

  void Update(mojom::TranscriptPartPtr transcript_part);

  const std::string session_id_;
  const std::string sender_email_;

  int64_t init_timestamp_ms_ = -1;
  int64_t transcript_id_;
  int64_t order_;
  std::string language_;
  std::string text_;
  int64_t text_index_;
  bool is_final_;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSCRIPT_BUILDER_H_
