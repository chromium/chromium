// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SYNC_ENCRYPTION_KEYS_EXTENSION_H_
#define CHROME_RENDERER_SYNC_ENCRYPTION_KEYS_EXTENSION_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/sync_encryption_keys_extension.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "v8/include/v8.h"

namespace gin {
class Arguments;
}  // namespace gin

// This class adds a function chrome.setSyncEncryptionKeys().
class SyncEncryptionKeysExtension : public content::RenderFrameObserver {
 public:
  // Creates a new instance, with ownership transferred to |*frame|.
  static void Create(content::RenderFrame* frame);
  ~SyncEncryptionKeysExtension() override;

  // content::RenderFrameObserver:
  void OnDestruct() override;
  void DidCreateScriptContext(v8::Local<v8::Context> v8_context,
                              int32_t world_id) override;

 private:
  explicit SyncEncryptionKeysExtension(content::RenderFrame* frame);

  void Install();
  void SetSyncEncryptionKeys(gin::Arguments* args);
  void RunCompletionCallback(
      std::unique_ptr<v8::Global<v8::Function>> callback);

  mojo::AssociatedRemote<chrome::mojom::SyncEncryptionKeysExtension> remote_;
  base::WeakPtrFactory<SyncEncryptionKeysExtension> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SyncEncryptionKeysExtension);
};

#endif  // CHROME_RENDERER_SYNC_ENCRYPTION_KEYS_EXTENSION_H_
