// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/boca/babelorca/cpp/proto_http_stream_parser.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/boca/babelorca/proto/stream_body.pb.h"
#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom-shared.h"
#include "net/base/io_buffer.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"

namespace ash::babelorca {
namespace {

constexpr int kReadBufferSpareCapacity = 2 * 1024;
// The tag type is the low 3 bits of the tag.
// https://protobuf.dev/programming-guides/encoding/#structure
constexpr int kTagTypeBits = 3;

// Wire types enum. SGROUP and EGROUP are not used (values 3 and 4).
enum WireType { kVarInt, kI64, kLen, kI32 = 5 };

bool IsValidWireType(int wire_type) {
  return wire_type == WireType::kVarInt || wire_type == WireType::kI64 ||
         wire_type == WireType::kLen || wire_type == WireType::kI32;
}

int GetWireType(uint32_t tag) {
  constexpr int kTagTypeMask = (1 << kTagTypeBits) - 1;
  return tag & kTagTypeMask;
}

int GetFieldNumber(uint32_t tag) {
  return tag >> kTagTypeBits;
}

}  // namespace

ProtoHttpStreamParser::ProtoHttpStreamParser(size_t max_pending_size)
    : max_pending_size_(max_pending_size),
      read_buffer_(base::MakeRefCounted<net::GrowableIOBuffer>()) {}

ProtoHttpStreamParser::~ProtoHttpStreamParser() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

mojom::ParsingState ProtoHttpStreamParser::Append(std::string_view data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (current_state_ != mojom::ParsingState::kOk) {
    return current_state_;
  }
  if (read_buffer_->RemainingCapacity() < static_cast<int>(data.size())) {
    read_buffer_->SetCapacity(read_buffer_->offset() + data.size() +
                              kReadBufferSpareCapacity);
  }
  CHECK_GE(read_buffer_->RemainingCapacity(), static_cast<int>(data.size()));
  memcpy(read_buffer_->data(), data.data(), data.size());
  read_buffer_->set_offset(read_buffer_->offset() + data.size());
  Parse();
  return current_state_;
}

void ProtoHttpStreamParser::Parse() {
  CHECK(read_buffer_);

  if (read_buffer_->offset() == 0) {
    return;
  }
  auto current_data = read_buffer_->span_before_offset();
  google::protobuf::io::CodedInputStream input_stream(current_data.data(),
                                                      current_data.size());
  int bytes_consumed = 0;
  bool parse_next = true;
  while (parse_next && bytes_consumed < read_buffer_->offset()) {
    parse_next = ParseOneField(&input_stream);
    if (parse_next) {
      // `parse_next` will be false in case of a parsed Status field but no need
      // to update `bytes_consumed` since no new data will be accepted by
      // `Append` in this case and no more data will be parsed.
      bytes_consumed = input_stream.CurrentPosition();
    }
  }
  if (current_state_ != mojom::ParsingState::kOk) {
    read_buffer_.reset();
    return;
  }
  auto unconsumed_data =
      read_buffer_->span_before_offset().subspan(bytes_consumed);
  if (unconsumed_data.size() > max_pending_size_) {
    current_state_ = mojom::ParsingState::kError;
    read_buffer_.reset();
    return;
  }
  read_buffer_->everything().copy_prefix_from(unconsumed_data);
  read_buffer_->set_offset(unconsumed_data.size());
}

std::vector<std::string> ProtoHttpStreamParser::TakeParseResult() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::string> result = std::move(parse_result_);
  parse_result_.clear();
  return result;
}

bool ProtoHttpStreamParser::ParseOneField(
    google::protobuf::io::CodedInputStream* input_stream) {
  uint32_t tag = input_stream->ReadTag();
  if (tag == 0) {
    // tag is not fully available yet.
    return false;
  }
  int wire_type = GetWireType(tag);
  if (!IsValidWireType(wire_type)) {
    current_state_ = mojom::ParsingState::kError;
    return false;
  }
  int field_number = GetFieldNumber(tag);
  if (field_number == 0) {
    // Invalid field number.
    current_state_ = mojom::ParsingState::kError;
    return false;
  } else if (field_number != StreamBody::kMessagesFieldNumber &&
             field_number != StreamBody::kStatusFieldNumber) {
    return SkipField(input_stream, wire_type);
  }
  // Field is either status or messages.
  if (wire_type != WireType::kLen) {
    current_state_ = mojom::ParsingState::kError;
    return false;
  }
  std::string message;
  uint32_t message_length;
  if (!input_stream->ReadVarint32(&message_length) ||
      !input_stream->ReadString(&message, message_length)) {
    return false;
  }
  parse_result_.push_back(std::move(message));
  if (field_number == StreamBody::kStatusFieldNumber) {
    current_state_ = mojom::ParsingState::kClosed;
    return false;
  }
  return true;
}

bool ProtoHttpStreamParser::SkipField(
    google::protobuf::io::CodedInputStream* input_stream,
    int wire_type) {
  switch (wire_type) {
    case WireType::kVarInt: {
      uint64_t value;
      return input_stream->ReadVarint64(&value);
    }
    case WireType::kI64: {
      uint64_t value;
      return input_stream->ReadLittleEndian64(&value);
    }
    case WireType::kLen: {
      uint32_t length;
      return input_stream->ReadVarint32(&length) && input_stream->Skip(length);
    }
    case WireType::kI32: {
      uint32_t value;
      return input_stream->ReadLittleEndian32(&value);
    }
    default: {
      current_state_ = mojom::ParsingState::kError;
      return false;
    }
  }
}

}  // namespace ash::babelorca
