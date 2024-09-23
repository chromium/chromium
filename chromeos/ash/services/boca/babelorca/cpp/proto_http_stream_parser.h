// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BOCA_BABELORCA_CPP_PROTO_HTTP_STREAM_PARSER_H_
#define CHROMEOS_ASH_SERVICES_BOCA_BABELORCA_CPP_PROTO_HTTP_STREAM_PARSER_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom-shared.h"

namespace google::protobuf::io {
class CodedInputStream;
}  // namespace google::protobuf::io

namespace net {
class GrowableIOBuffer;
}  // namespace net

namespace ash::babelorca {

// Class to parse incoming data stream wrapped in a StreamBody proto message.
class ProtoHttpStreamParser {
 public:
  explicit ProtoHttpStreamParser(size_t max_pending_size = 2 * 1024);

  ProtoHttpStreamParser(const ProtoHttpStreamParser&) = delete;
  ProtoHttpStreamParser& operator=(const ProtoHttpStreamParser&) = delete;

  ~ProtoHttpStreamParser();

  // Appends the stream data if current parsing state is
  // `mojom::ParsingState::kOk`, and produces parsed data if any data can be
  // parsed. Updates and returns parsing state if it was
  // `mojom::ParsingState::kOk` before the call, and returns the parsing state
  // directly otherwise.
  mojom::ParsingState Append(std::string_view data);

  // Returns and clear parsing results.
  std::vector<std::string> TakeParseResult();

 private:
  // Parses any current data that can be parsed.
  void Parse();

  // Returns true if the field was parsed and it is not a stream `Status` field.
  bool ParseOneField(google::protobuf::io::CodedInputStream* input_stream);

  // Returns true if the field was skipped.
  bool SkipField(google::protobuf::io::CodedInputStream* input_stream,
                 int wire_type);

  const size_t max_pending_size_;

  SEQUENCE_CHECKER(sequence_checker_);

  std::vector<std::string> parse_result_;

  mojom::ParsingState current_state_ = mojom::ParsingState::kOk;

  scoped_refptr<net::GrowableIOBuffer> read_buffer_;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_SERVICES_BOCA_BABELORCA_CPP_PROTO_HTTP_STREAM_PARSER_H_
