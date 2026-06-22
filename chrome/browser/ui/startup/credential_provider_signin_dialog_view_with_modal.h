// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_CREDENTIAL_PROVIDER_SIGNIN_DIALOG_VIEW_WITH_MODAL_H_
#define CHROME_BROWSER_UI_STARTUP_CREDENTIAL_PROVIDER_SIGNIN_DIALOG_VIEW_WITH_MODAL_H_

#include <memory>

#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "ui/views/controls/webview/web_dialog_view.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace web_modal {
class WebContentsModalDialogManager;
}

class WebContentsHandler;

// This class is created to support showing modal dialogs
// for webauthn. This class overrides WebContentsModalDialogManagerDelegate.
// please see:
// https://cs.chromium.org/chromium/src/chrome/browser/ui/views/webauthn/authenticator_request_dialog_view_controller_views.cc;l=35
class CredentialProviderWebDialogViewWithModal
    : public views::WebDialogView,
      public web_modal::WebContentsModalDialogManagerDelegate,
      public web_modal::WebContentsModalDialogHost {
 public:
  CredentialProviderWebDialogViewWithModal(
      content::BrowserContext* context,
      ui::WebDialogDelegate* delegate,
      std::unique_ptr<WebContentsHandler> handler);

  CredentialProviderWebDialogViewWithModal(
      const CredentialProviderWebDialogViewWithModal&) = delete;
  CredentialProviderWebDialogViewWithModal& operator=(
      const CredentialProviderWebDialogViewWithModal&) = delete;

  ~CredentialProviderWebDialogViewWithModal() override;

  bool IsVisible();

  // views::WebDialogView:
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  bool IsWebContentsCreationOverridden(
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) override;

  content::WebContents* CreateCustomWebContents(
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      bool is_new_browsing_instance,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      const content::StoragePartitionConfig& partition_config,
      content::SessionStorageNamespace* session_storage_namespace) override;

 private:
  // web_modal::WebContentsModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost(
      content::WebContents* web_contents) override;
  bool IsWebContentsVisible(content::WebContents* web_contents) override;

  // web_modal::WebContentsModalDialogHost:
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

  raw_ptr<web_modal::WebContentsModalDialogManager> modal_dialog_manager_;
};

#endif  // CHROME_BROWSER_UI_STARTUP_CREDENTIAL_PROVIDER_SIGNIN_DIALOG_VIEW_WITH_MODAL_H_
