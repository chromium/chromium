// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LANGUAGE_LANGUAGE_PACKS_LANGUAGE_PACKS_IMPL_H_
#define CHROMEOS_LANGUAGE_LANGUAGE_PACKS_LANGUAGE_PACKS_IMPL_H_

#include <string>

#include "chromeos/language/language_packs/language_pack_manager.h"
#include "chromeos/language/public/mojom/language_packs.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos::language_packs {

class LanguagePacksImpl : public chromeos::language::mojom::LanguagePacks {
 public:
  // Singleton getter
  static LanguagePacksImpl& GetInstance();

  LanguagePacksImpl();
  LanguagePacksImpl(const LanguagePacksImpl&) = delete;
  LanguagePacksImpl& operator=(const LanguagePacksImpl&) = delete;
  // Called when mojom connection is destroyed.
  ~LanguagePacksImpl() override;

  void BindReceiver(
      mojo::PendingReceiver<language::mojom::LanguagePacks> receiver);

  // mojom::LanguagePacks interface methods.
  void GetPackInfo(chromeos::language::mojom::FeatureId feature_id,
                   const std::string& language,
                   GetPackInfoCallback callback) override;
  void InstallPack(chromeos::language::mojom::FeatureId feature_id,
                   const std::string& language,
                   InstallPackCallback callback) override;

 private:
  mojo::ReceiverSet<chromeos::language::mojom::LanguagePacks> receivers_;
};

}  // namespace chromeos::language_packs

#endif  // CHROMEOS_LANGUAGE_LANGUAGE_PACKS_LANGUAGE_PACKS_IMPL_H_
