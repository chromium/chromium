// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_TRUSTED_VAULT_ENCRYPTION_KEYS_EXTENSION_H_
#define CHROME_RENDERER_TRUSTED_VAULT_ENCRYPTION_KEYS_EXTENSION_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/common/trusted_vault_encryption_keys_extension.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"

namespace gin {
class Arguments;
}  // namespace gin

// This class installs private APIs on Google Accounts origins that configure
// on-device encryption keys for //components/trusted_vault.
class TrustedVaultEncryptionKeysExtension
    : public content::RenderFrameObserver {
 public:
  // Creates a new instance, with ownership transferred to |*frame|.
  static void Create(content::RenderFrame* frame);

  TrustedVaultEncryptionKeysExtension(
      const TrustedVaultEncryptionKeysExtension&) = delete;
  TrustedVaultEncryptionKeysExtension& operator=(
      const TrustedVaultEncryptionKeysExtension&) = delete;

  ~TrustedVaultEncryptionKeysExtension() override;

  // content::RenderFrameObserver:
  void OnDestruct() override;
  void DidCreateScriptContext(v8::Local<v8::Context> v8_context,
                              int32_t world_id) override;

 private:
  explicit TrustedVaultEncryptionKeysExtension(content::RenderFrame* frame);

  void Install();
#if !BUILDFLAG(IS_ANDROID)
  void SetSyncEncryptionKeys(gin::Arguments* args);
  void SetClientEncryptionKeys(gin::Arguments* args);
  void SetClientEncryptionKeysContinue(
      gin::Arguments* args,
      v8::Local<v8::Function> callback,
      std::string gaia_id,
      std::optional<
          base::flat_map<std::string,
                         std::vector<chrome::mojom::TrustedVaultKeyPtr>>>
          trusted_vault_keys);
#endif
  void AddTrustedSyncEncryptionRecoveryMethod(gin::Arguments* args);
  void RunCompletionCallback(
      std::unique_ptr<v8::Global<v8::Function>> callback);

  mojo::AssociatedRemote<chrome::mojom::TrustedVaultEncryptionKeysExtension>
      remote_;
  base::WeakPtrFactory<TrustedVaultEncryptionKeysExtension> weak_ptr_factory_{
      this};
};

#endif  // CHROME_RENDERER_TRUSTED_VAULT_ENCRYPTION_KEYS_EXTENSION_H_
