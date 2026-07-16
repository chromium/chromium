// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PASSWORD_MANAGER_REMOTE_ACTOR_CREDENTIAL_SHARING_EXTENSION_H_
#define CHROME_RENDERER_PASSWORD_MANAGER_REMOTE_ACTOR_CREDENTIAL_SHARING_EXTENSION_H_

#include "base/memory/weak_ptr.h"
#include "chrome/common/password_manager/remote_actor_credential_sharing.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "v8/include/v8-forward.h"

namespace content {
class RenderFrame;
}  // namespace content

// Exposes the `chrome.requestAgentAuthentication` JavaScript API to
// authorized main frame origins.
class RemoteActorCredentialSharingExtension
    : public content::RenderFrameObserver {
 public:
  static void Create(content::RenderFrame* render_frame);

  explicit RemoteActorCredentialSharingExtension(
      content::RenderFrame* render_frame);
  ~RemoteActorCredentialSharingExtension() override;
  RemoteActorCredentialSharingExtension(
      const RemoteActorCredentialSharingExtension&) = delete;
  RemoteActorCredentialSharingExtension& operator=(
      const RemoteActorCredentialSharingExtension&) = delete;

  // content::RenderFrameObserver:
  void DidCreateScriptContext(v8::Local<v8::Context> context,
                              int32_t world_id) override;
  void OnDestruct() override;

 private:
  // Installs the javascript bindings.
  void Install(v8::Local<v8::Context> context);

  // Handles the Javascript call to requestAgentAuthentication.
  void RequestAgentAuthentication(const std::string& gaia_id,
                                  const std::string& domain,
                                  const std::string& remote_actor_id,
                                  v8::Local<v8::Function> callback_function);

  // Runs the Javascript callback on completion.
  void RunCompletionCallback(std::unique_ptr<v8::Global<v8::Function>> callback,
                             bool success);

  // Helper method to retrieve the remote mojo interface host.
  chrome::mojom::RemoteActorCredentialSharing& GetRemoteInterface();

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing>
      remote_interface_;

  base::WeakPtrFactory<RemoteActorCredentialSharingExtension> weak_ptr_factory_{
      this};
};

#endif  // CHROME_RENDERER_PASSWORD_MANAGER_REMOTE_ACTOR_CREDENTIAL_SHARING_EXTENSION_H_
