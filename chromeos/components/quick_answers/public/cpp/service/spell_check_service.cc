// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/public/cpp/service/spell_check_service.h"

#include "base/logging.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace quick_answers {

SpellCheckService::SpellCheckService(
    mojo::PendingReceiver<quick_answers::mojom::SpellCheckService>
        pending_receiver)
    : receiver_(this, std::move(pending_receiver)) {}

SpellCheckService::~SpellCheckService() = default;

void SpellCheckService::CreateDictionary(base::File file,
                                         CreateDictionaryCallback callback) {
  auto dictionary = std::make_unique<SpellCheckDictionary>();
  if (!dictionary->Initialize(std::move(file))) {
    std::move(callback).Run(std::move(mojo::NullRemote()));
    return;
  }

  mojo::PendingRemote<mojom::SpellCheckDictionary> pending_remote;
  mojo::MakeSelfOwnedReceiver(std::move(dictionary),
                              pending_remote.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(std::move(pending_remote));
}

}  // namespace quick_answers
