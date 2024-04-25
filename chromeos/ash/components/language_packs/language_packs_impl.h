// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LANGUAGE_PACKS_LANGUAGE_PACKS_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_LANGUAGE_PACKS_LANGUAGE_PACKS_IMPL_H_

#include <string>

#include "chromeos/ash/components/language_packs/language_pack_manager.h"
#include "chromeos/ash/components/language_packs/public/mojom/language_packs.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::language_packs {

class LanguagePacksImpl : public ash::language::mojom::LanguagePacks,
                          public LanguagePackManager::Observer {
 public:
  // Singleton getter
  static LanguagePacksImpl& GetInstance();

  LanguagePacksImpl();
  LanguagePacksImpl(const LanguagePacksImpl&) = delete;
  LanguagePacksImpl& operator=(const LanguagePacksImpl&) = delete;
  // Called when mojom connection is destroyed.
  ~LanguagePacksImpl() override;

  void BindReceiver(
      mojo::PendingReceiver<ash::language::mojom::LanguagePacks> receiver);

  // LanguagePackManager::Observer:
  void OnPackStateChanged(const PackResult& pack_result) override;

  // mojom::LanguagePacks interface methods.
  void GetPackInfo(ash::language::mojom::FeatureId feature_id,
                   const std::string& language,
                   GetPackInfoCallback callback) override;
  void InstallPack(ash::language::mojom::FeatureId feature_id,
                   const std::string& language,
                   InstallPackCallback callback) override;
  void InstallBasePack(ash::language::mojom::FeatureId feature_id,
                       InstallBasePackCallback callback) override;
  void UninstallPack(ash::language::mojom::FeatureId feature_id,
                     const std::string& language,
                     UninstallPackCallback callbck) override;
  void AddObserver(
      mojo::PendingAssociatedRemote<ash::language::mojom::LanguagePacksObserver>
          observer) override;

 private:
  mojo::ReceiverSet<ash::language::mojom::LanguagePacks> receivers_;
  mojo::AssociatedRemoteSet<ash::language::mojom::LanguagePacksObserver>
      observers_;
};

}  // namespace ash::language_packs

#endif  // CHROMEOS_ASH_COMPONENTS_LANGUAGE_PACKS_LANGUAGE_PACKS_IMPL_H_
