// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_MESSAGE_H_
#define CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_MESSAGE_H_

#include <string>
#include "base/types/expected.h"
#include "base/values.h"
#include "chromeos/ash/components/quick_start/quick_start_message_type.h"

namespace ash::quick_start {

// Wraps a QuickStartMessage, which is a JSON object with payload which
// represents either a request or a response. This class collects common logic
// used by both request builders and parsers.
class QuickStartMessage {
 public:
  enum class ReadError {
    INVALID_JSON,
    MISSING_MESSAGE_PAYLOAD,
    BASE64_DESERIALIZATION_FAILURE,
    UNEXPECTED_MESSAGE_TYPE,
  };

  using ReadResult =
      base::expected<std::unique_ptr<QuickStartMessage>, ReadError>;

  explicit QuickStartMessage(QuickStartMessageType message_type);
  QuickStartMessage(QuickStartMessageType message_type,
                    base::Value::Dict payload);
  QuickStartMessage(QuickStartMessage&) = delete;
  QuickStartMessage& operator=(QuickStartMessage&) = delete;
  ~QuickStartMessage();

  base::Value::Dict* GetPayload();
  QuickStartMessageType get_type() { return message_type_; }
  std::unique_ptr<base::Value::Dict> GenerateEncodedMessage();

  // Read a message from raw data.
  // NOTE: This function must be called in a process isolated from the
  // browser process - it will fail otherwise.
  static base::expected<std::unique_ptr<QuickStartMessage>,
                        QuickStartMessage::ReadError>
  ReadMessage(std::vector<uint8_t> data);
  static base::expected<std::unique_ptr<QuickStartMessage>,
                        QuickStartMessage::ReadError>
  ReadMessage(std::vector<uint8_t> data, QuickStartMessageType message_type);

  static void DisableSandboxCheckForTesting();

 private:
  // enables checks for ReadMessage which force the function to only run in a
  // sandbox
  static bool enable_sandbox_checks_;
  QuickStartMessageType message_type_;
  base::Value::Dict payload_;
};

}  // namespace ash::quick_start

#endif  // CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_MESSAGE_H_
