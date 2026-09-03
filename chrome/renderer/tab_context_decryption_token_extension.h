// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_TAB_CONTEXT_DECRYPTION_TOKEN_EXTENSION_H_
#define CHROME_RENDERER_TAB_CONTEXT_DECRYPTION_TOKEN_EXTENSION_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/common/tab_context_decryption_token_extension.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "url/origin.h"
#include "v8/include/v8-array-buffer.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"

// This class installs chrome.getContainerDecryptionToken on allowed tab context
// origins to request tab context container decryption tokens.
class TabContextDecryptionTokenExtension : public content::RenderFrameObserver {
 public:
  static void Create(content::RenderFrame* frame);

  TabContextDecryptionTokenExtension(
      const TabContextDecryptionTokenExtension&) = delete;
  TabContextDecryptionTokenExtension& operator=(
      const TabContextDecryptionTokenExtension&) = delete;

  ~TabContextDecryptionTokenExtension() override;

  // content::RenderFrameObserver:
  void OnDestruct() override;
  void DidCreateScriptContext(v8::Local<v8::Context> v8_context,
                              int32_t world_id) override;

  static bool ShouldExposeTabContextJavascriptApiForTesting(
      const url::Origin& origin,
      bool is_locked_to_site);

  static v8::Local<v8::Value> CreateTokenValueForTesting(
      v8::Isolate* isolate,
      const std::optional<std::vector<uint8_t>>& token_bytes);

 private:
  explicit TabContextDecryptionTokenExtension(content::RenderFrame* frame);

  static bool ShouldExposeTabContextJavascriptApi(const url::Origin& origin,
                                                  bool is_locked_to_site);

  static v8::Local<v8::Value> CreateTokenValue(
      v8::Isolate* isolate,
      const std::optional<std::vector<uint8_t>>& token_bytes);

  void Install();
  void GetContainerDecryptionToken(v8::Local<v8::Function> callback,
                                   const std::string& obfuscated_gaia_id,
                                   const std::string& container_id_str);
  void OnGetContainerDecryptionTokenResponse(
      std::unique_ptr<v8::Global<v8::Function>> callback,
      const std::optional<std::vector<uint8_t>>& token_bytes);

  mojo::AssociatedRemote<chrome::mojom::TabContextDecryptionTokenExtension>
      remote_;
  base::WeakPtrFactory<TabContextDecryptionTokenExtension> weak_ptr_factory_{
      this};
};

#endif  // CHROME_RENDERER_TAB_CONTEXT_DECRYPTION_TOKEN_EXTENSION_H_
