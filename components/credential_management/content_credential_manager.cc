// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/credential_management/content_credential_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/password_manager/core/browser/credential_manager_impl.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"

namespace {
void LogGetCredentialsMetrics(
    credential_management::ContentCredentialManager::GetCallback callback,
    password_manager::CredentialManagerError error,
    const std::optional<password_manager::CredentialInfo>& info) {
  password_manager::metrics_util::LogCumulativeGetCredentialsMetrics(error);
  std::move(callback).Run(error, info);
}
}  // namespace

namespace credential_management {

// ContentCredentialManager -------------------------------------------------

ContentCredentialManager::ContentCredentialManager(
    std::unique_ptr<CredentialManagerInterface> credential_manager)
    : credential_manager_(std::move(credential_manager)) {}

ContentCredentialManager::~ContentCredentialManager() = default;

void ContentCredentialManager::BindRequest(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::CredentialManager> receiver) {
  // Only valid for the main frame.
  if (frame_host->GetParent()) {
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame_host);
  DCHECK(web_contents);

  // Only valid for the currently committed RenderFrameHost, and not, e.g. old
  // zombie RFH's being swapped out following cross-origin navigations.
  if (web_contents->GetPrimaryMainFrame() != frame_host) {
    return;
  }

  // The ContentCredentialManager will not bind the mojo interface for
  // non-primary frames, e.g. BackForwardCache, Prerenderer, since the
  // MojoBinderPolicy prevents this interface from being granted.
  DCHECK_EQ(frame_host->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kActive);

  // Disable BackForwardCache for this page.
  // This is necessary because ContentCredentialManager::DisconnectBinding()
  // will be called when the page is navigated away from, leaving it
  // in an unusable state if the page is restored from the BackForwardCache.
  //
  // It looks like in order to remove this workaround, we probably just need to
  // make the CredentialManager mojo API rebind on the renderer side when the
  // next call is made, if it has become disconnected.
  // TODO(crbug.com/40653684): Remove this workaround.
  content::BackForwardCache::DisableForRenderFrameHost(
      frame_host, back_forward_cache::DisabledReason(
                      back_forward_cache::DisabledReasonId::
                          kContentCredentialManager_BindCredentialManager));

  if (receiver_.is_bound()) {
    mojo::ReportBadMessage("CredentialManager is already bound.");
    return;
  }
  receiver_.Bind(std::move(receiver));

  // The browser side will close the message pipe on DidFinishNavigation before
  // the renderer side would be destroyed, and the renderer never explicitly
  // closes the pipe. So a connection error really means an error here, in which
  // case the renderer will try to reconnect when the next call to the API is
  // made. Make sure this implementation will no longer be bound to a broken
  // pipe once that happens, so the DCHECK above will succeed.
  receiver_.set_disconnect_handler(base::BindOnce(
      &ContentCredentialManager::DisconnectBinding, base::Unretained(this)));
}

bool ContentCredentialManager::HasBinding() const {
  return receiver_.is_bound();
}

void ContentCredentialManager::DisconnectBinding() {
  receiver_.reset();
  credential_manager_->ResetAfterDisconnecting();
}

void ContentCredentialManager::Store(
    const password_manager::CredentialInfo& credential,
    StoreCallback callback) {
  credential_manager_->Store(credential, std::move(callback));
}

void ContentCredentialManager::PreventSilentAccess(
    PreventSilentAccessCallback callback) {
  credential_manager_->PreventSilentAccess(std::move(callback));
}

void ContentCredentialManager::Get(
    password_manager::CredentialMediationRequirement mediation,
    bool include_passwords,
    const std::vector<GURL>& federations,
    GetCallback callback) {
  credential_manager_->Get(
      mediation, include_passwords, federations,
      base::BindOnce(&LogGetCredentialsMetrics, std::move(callback)));
}

}  // namespace credential_management
