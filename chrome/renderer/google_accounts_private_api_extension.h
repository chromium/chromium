// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_GOOGLE_ACCOUNTS_PRIVATE_API_EXTENSION_H_
#define CHROME_RENDERER_GOOGLE_ACCOUNTS_PRIVATE_API_EXTENSION_H_

#include "base/memory/weak_ptr.h"
#include "chrome/common/google_accounts_private_api_extension.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "v8/include/v8-forward.h"

namespace gin {
class Arguments;
}  // namespace gin

// This class allows the addition of functions to the Google Accounts page;
// accounts.google.com.
class GoogleAccountsPrivateApiExtension : public content::RenderFrameObserver {
 public:
  // Creates a new instance, with ownership transferred to |*frame|.
  static void Create(content::RenderFrame* frame);

  GoogleAccountsPrivateApiExtension(const GoogleAccountsPrivateApiExtension&) =
      delete;
  GoogleAccountsPrivateApiExtension& operator=(
      const GoogleAccountsPrivateApiExtension&) = delete;

  ~GoogleAccountsPrivateApiExtension() override;

  // content::RenderFrameObserver:
  void OnDestruct() override;
  void DidCreateScriptContext(v8::Local<v8::Context> v8_context,
                              int32_t world_id) override;

 private:
  explicit GoogleAccountsPrivateApiExtension(content::RenderFrame* frame);

  void InjectScript();

#if !BUILDFLAG(IS_ANDROID)
  void SetConsentResult(gin::Arguments* args);
#endif  // !BUILDFLAG(IS_ANDROID)

  mojo::AssociatedRemote<chrome::mojom::GoogleAccountsPrivateApiExtension>
      remote_;
  base::WeakPtrFactory<GoogleAccountsPrivateApiExtension> weak_ptr_factory_{
      this};
};

#endif  // CHROME_RENDERER_GOOGLE_ACCOUNTS_PRIVATE_API_EXTENSION_H_
