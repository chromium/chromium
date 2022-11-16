// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/rule_based_engine_connection_factory.h"

#include <utility>

namespace ash {
namespace ime {

RuleBasedEngineConnectionFactory::RuleBasedEngineConnectionFactory(
    mojo::PendingReceiver<mojom::ConnectionFactory> pending_receiver)
    : receiver_(this, std::move(pending_receiver)) {}

RuleBasedEngineConnectionFactory::~RuleBasedEngineConnectionFactory() = default;

void RuleBasedEngineConnectionFactory::ConnectToInputMethod(
    const std::string& ime_spec,
    mojo::PendingAssociatedReceiver<mojom::InputMethod> pending_input_method,
    mojo::PendingAssociatedRemote<mojom::InputMethodHost>
        pending_input_method_host,
    mojom::InputMethodSettingsPtr settings,
    ConnectToInputMethodCallback callback) {
  // `settings` is unused because Rule-Based IMEs do not have settings.
  rule_based_engine_ =
      RuleBasedEngine::Create(ime_spec, std::move(pending_input_method),
                              std::move(pending_input_method_host));
  std::move(callback).Run(/*bound=*/true);
}

void RuleBasedEngineConnectionFactory::ConnectToJapaneseDecoder(
    mojo::PendingAssociatedReceiver<mojom::JapaneseDecoder> pending_reciever,
    ConnectToJapaneseDecoderCallback callback) {
  // Connecting to the Mozc engine with the Rulebased connection
  // engine is not supported.
  NOTIMPLEMENTED_LOG_ONCE();
  std::move(callback).Run(false);
}

bool RuleBasedEngineConnectionFactory::IsConnected() {
  return rule_based_engine_ && rule_based_engine_->IsConnected();
}

}  // namespace ime
}  // namespace ash
