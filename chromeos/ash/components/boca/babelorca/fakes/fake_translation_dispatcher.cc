// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/fakes/fake_translation_dispatcher.h"

#include "components/live_caption/translation_dispatcher.h"

namespace ash::babelorca {

FakeBabelOrcaTranslationDispatcher::FakeBabelOrcaTranslationDispatcher() =
    default;
FakeBabelOrcaTranslationDispatcher::~FakeBabelOrcaTranslationDispatcher() =
    default;

void FakeBabelOrcaTranslationDispatcher::GetTranslation(
    const std::string& result,
    const std::string& source_language,
    const std::string& target_language,
    captions::OnTranslateEventCallback callback) {
  ++num_translation_calls_;
  std::move(callback).Run(result);
}

}  // namespace ash::babelorca
