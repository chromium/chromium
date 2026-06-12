// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/credential_provider_signin_dialog_view_with_modal.h"

#include "base/logging.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"

CredentialProviderWebDialogViewWithModal::
    CredentialProviderWebDialogViewWithModal(
        content::BrowserContext* context,
        ui::WebDialogDelegate* delegate,
        std::unique_ptr<WebContentsHandler> handler)
    : views::WebDialogView(context, delegate, std::move(handler)) {}

CredentialProviderWebDialogViewWithModal::
    ~CredentialProviderWebDialogViewWithModal() = default;

void CredentialProviderWebDialogViewWithModal::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  views::WebDialogView::ViewHierarchyChanged(details);
  if (details.is_add && GetWidget() && !modal_dialog_manager_) {
    // Get the existing manager if it exists. If it doesn't, create it.
    auto* manager = web_modal::WebContentsModalDialogManager::FromWebContents(
        web_contents());
    if (!manager) {
      web_modal::WebContentsModalDialogManager::CreateForWebContents(
          web_contents());
      manager = web_modal::WebContentsModalDialogManager::FromWebContents(
          web_contents());
    }
    modal_dialog_manager_ = manager;
    DCHECK(modal_dialog_manager_);
    modal_dialog_manager_->SetDelegate(this);
  }
}

bool CredentialProviderWebDialogViewWithModal::IsWebContentsCreationOverridden(
    content::RenderFrameHost* opener,
    content::SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  return true;
}

content::WebContents*
CredentialProviderWebDialogViewWithModal::CreateCustomWebContents(
    content::RenderFrameHost* opener,
    content::SiteInstance* source_site_instance,
    bool is_new_browsing_instance,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    const content::StoragePartitionConfig& partition_config,
    content::SessionStorageNamespace* session_storage_namespace) {
  VLOG(0) << "Suppressed window creation for  " << target_url.GetHost()
          << target_url.GetPath();
  return nullptr;
}

web_modal::WebContentsModalDialogHost*
CredentialProviderWebDialogViewWithModal::GetWebContentsModalDialogHost(
    content::WebContents* web_contents) {
  return this;
}

bool CredentialProviderWebDialogViewWithModal::IsWebContentsVisible(
    content::WebContents* web_contents) {
  return GetWidget()->IsVisible();
}

gfx::NativeView CredentialProviderWebDialogViewWithModal::GetHostView() const {
  return GetWidget()->GetNativeView();
}

gfx::Point CredentialProviderWebDialogViewWithModal::GetDialogPosition(
    const gfx::Size& size) {
  const gfx::Size& host_size =
      GetWidget()->GetClientAreaBoundsInScreen().size();
  return gfx::Point(std::max(0, (host_size.width() - size.width()) / 2),
                    std::max(0, (host_size.height() - size.height()) / 2));
}

gfx::Size CredentialProviderWebDialogViewWithModal::GetMaximumDialogSize() {
  return GetWidget()->GetClientAreaBoundsInScreen().size();
}

void CredentialProviderWebDialogViewWithModal::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {}

void CredentialProviderWebDialogViewWithModal::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {}
