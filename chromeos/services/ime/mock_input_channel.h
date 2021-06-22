// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_MOCK_INPUT_CHANNEL_H_
#define CHROMEOS_SERVICES_IME_MOCK_INPUT_CHANNEL_H_

#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/services/ime/public/mojom/input_method.mojom.h"
#include "chromeos/services/ime/public/mojom/input_method_host.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
namespace ime {

// A mock receiver InputChannel.
class MockInputChannel : public mojom::InputChannel {
 public:
  MockInputChannel();
  ~MockInputChannel();
  MockInputChannel(const MockInputChannel&) = delete;
  MockInputChannel& operator=(const MockInputChannel&) = delete;

  mojo::PendingRemote<mojom::InputChannel> CreatePendingRemote();
  bool IsBound() const;
  void FlushForTesting();

  // mojom::InputChannel:
  void ProcessMessage(const std::vector<uint8_t>& message,
                      ProcessMessageCallback callback) override {
    std::move(callback).Run({});
  }
  MOCK_METHOD(void,
              CommitText,
              (const std::string& text,
               mojom::CommitTextCursorBehavior cursor_behavior),
              (override));
  MOCK_METHOD(void, SetComposition, (const std::string& text), (override));
  MOCK_METHOD(void,
              SetCompositionRange,
              (uint32_t start_byte_index, uint32_t end_byte_index),
              (override));
  MOCK_METHOD(void, FinishComposition, (), (override));
  MOCK_METHOD(void,
              DeleteSurroundingText,
              (uint32_t num_bytes_before_cursor,
               uint32_t num_bytes_after_cursor),
              (override));
  MOCK_METHOD(void,
              HandleAutocorrect,
              (mojom::AutocorrectSpanPtr autocorrect_span),
              (override));
  MOCK_METHOD(void,
              RequestSuggestions,
              (mojom::SuggestionsRequestPtr request,
               RequestSuggestionsCallback callback),
              (override));
  MOCK_METHOD(void,
              DisplaySuggestions,
              (const std::vector<ime::TextSuggestion>& suggestions),
              (override));
  MOCK_METHOD(void, RecordUkm, (mojom::UkmEntryPtr entry), (override));

 private:
  mojo::Receiver<mojom::InputChannel> receiver_;
};

}  // namespace ime
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_IME_MOCK_INPUT_CHANNEL_H_
