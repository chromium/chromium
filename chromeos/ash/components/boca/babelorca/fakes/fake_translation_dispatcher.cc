// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/fakes/fake_translation_dispatcher.h"

#include "base/functional/callback.h"
#include "base/location.h"
#include "components/live_caption/translation_util.h"

namespace ash::babelorca {

namespace {

void InvokeFakeDispatch(captions::TranslateEventCallback callback,
                        const std::string& result) {
  std::move(callback).Run(base::ok(result));
}

}  // namespace

FakeBabelOrcaTranslationDispatcher::FakeBabelOrcaTranslationDispatcher() =
    default;
FakeBabelOrcaTranslationDispatcher::~FakeBabelOrcaTranslationDispatcher() =
    default;

void FakeBabelOrcaTranslationDispatcher::GetTranslation(
    const std::string& result,
    const std::string& source_language,
    const std::string& target_language,
    captions::TranslateEventCallback callback) {
  ++num_translation_calls_;
  if (!dispatch_handler_.is_null()) {
    dispatch_handler_.Run(base::BindOnce(&InvokeFakeDispatch,
                                         std::move(callback),
                                         injected_result_.value_or(result)));
    return;
  }

  std::move(callback).Run(
      captions::TranslateEvent(injected_result_.value_or(result)));
}

}  // namespace ash::babelorca
