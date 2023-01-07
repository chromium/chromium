// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_SERVICE_SPELL_CHECK_SERVICE_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_SERVICE_SPELL_CHECK_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/components/quick_answers/public/cpp/service/spell_check_dictionary.h"
#include "chromeos/components/quick_answers/public/mojom/spell_check.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace quick_answers {

// Utility class for spell check ran in renderer process.
class SpellCheckService : public mojom::SpellCheckService {
 public:
  explicit SpellCheckService(
      mojo::PendingReceiver<mojom::SpellCheckService> pending_receiver);

  SpellCheckService(const SpellCheckService&) = delete;
  SpellCheckService& operator=(const SpellCheckService&) = delete;

  ~SpellCheckService() override;

  void CreateDictionary(base::File file,
                        CreateDictionaryCallback callback) override;

 private:
  std::unique_ptr<SpellCheckDictionary> dictionary_;

  mojo::Receiver<mojom::SpellCheckService> receiver_;
};

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_SERVICE_SPELL_CHECK_SERVICE_H_
