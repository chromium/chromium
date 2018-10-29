// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/web_contents_manager.h"

#include <memory>

#include "ash/public/cpp/app_list/answer_card_contents_registry.h"
#include "ash/public/interfaces/assistant_controller.mojom.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/renderer_preferences.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"

// ManagedWebContents ----------------------------------------------------------

// This class wraps a WebContents and associated view. ManagedWebContents are
// owned/managed by WebContentsManager on behalf of embedders in other code
// areas/processes that cannot depend directly on chrome/browser. Instances are
// created/destroyed via calls to WebContentsManager::ManageWebContents and
// WebContentsManager::ReleaseWebContents respectively.
class ManagedWebContents : public content::WebContentsDelegate,
                           public content::WebContentsObserver {
 public:
  ManagedWebContents(
      ash::mojom::ManagedWebContentsParamsPtr params,
      ash::mojom::WebContentsManager::ManageWebContentsCallback callback)
      : callback_(std::move(callback)), weak_factory_(this) {
    Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByAccountId(
        params->account_id);

    if (!profile) {
      LOG(WARNING) << "Unable to retrieve profile for account_id.";
      std::move(callback_).Run(base::nullopt);
      return;
    }

    InitWebContents(profile, std::move(params));
    HandleWebContents(profile);
  }

  ~ManagedWebContents() override {
    // We may be destroying this instance before |callback_| was signaled that
    // contents were ready for embedding. In such cases we still need to run the
    // callback as it is an error to drop response callbacks which still
    // correspond to an open interface pipe.
    if (callback_)
      std::move(callback_).Run(base::nullopt);

    web_contents_->SetDelegate(nullptr);
    Observe(nullptr);

    // When WebContents are rendered in the same process as ash, we need to
    // release the associated view registered in the
    // AnswerCardContentsRegistry's token-to-view map.
    if (app_list::AnswerCardContentsRegistry::Get() &&
        embed_token_.has_value()) {
      app_list::AnswerCardContentsRegistry::Get()->Unregister(
          embed_token_.value());
    }
  }

  // content::WebContentsDelegate:
  void ResizeDueToAutoResize(content::WebContents* web_contents,
                             const gfx::Size& new_size) override {
    web_view_->SetPreferredSize(new_size);
  }

  bool ShouldCreateWebContents(
      content::WebContents* web_contents,
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      int32_t route_id,
      int32_t main_frame_route_id,
      int32_t main_frame_widget_route_id,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      content::SessionStorageNamespace* session_storage_namespace) override {
    // This is fired when trying to open links in a new tab, e.g.:
    // <a href="https://www.google.com/" target="_blank">Link</a>
    HandleNavigationAttempt(target_url,
                            WindowOpenDisposition::NEW_FOREGROUND_TAB);
    return false;
  }

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override {
    HandleNavigationAttempt(params.url, params.disposition);
    return nullptr;
  }

  // content::WebContentsObserver:
  void DidStopLoading() override {
    // After the first load has stopped, notify |callback_| that we're ready for
    // embedding. We wait until load completion to reduce UI jank.
    if (callback_)
      std::move(callback_).Run(embed_token_);
  }

  void NavigateBack(
      ash::mojom::WebContentsManager::NavigateWebContentsBackCallback
          callback) {
    content::NavigationController& controller = web_contents_->GetController();
    if (controller.CanGoBack()) {
      controller.GoBack();
      std::move(callback).Run(true);
    } else {
      std::move(callback).Run(false);
    }
  }

  void NavigateToUrl(const GURL& url) {
    content::NavigationController::LoadURLParams params(url);
    web_contents_->GetController().LoadURLWithParams(params);
  }

 private:
  void InitWebContents(Profile* profile,
                       ash::mojom::ManagedWebContentsParamsPtr params) {
    web_contents_ =
        content::WebContents::Create(content::WebContents::CreateParams(
            profile, content::SiteInstance::Create(profile)));

    // If delegate info is provided, intercept navigation attempts for top level
    // browser requests. These events will be forwarded to the delegate.
    if (params->open_url_delegate_ptr_info.is_valid()) {
      open_url_delegate_.Bind(std::move(params->open_url_delegate_ptr_info));
      content::RendererPreferences* renderer_prefs =
          web_contents_->GetMutableRendererPrefs();
      renderer_prefs->browser_handles_all_top_level_requests = true;
      web_contents_->GetRenderViewHost()->SyncRendererPrefs();
    }

    // Use a transparent background.
    views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
        web_contents_.get(), SK_ColorTRANSPARENT);

    Observe(web_contents_.get());
    web_contents_->SetDelegate(this);

    // Load the desired URL into the web contents.
    content::NavigationController::LoadURLParams load_params(params->url);
    load_params.should_clear_history_list = true;
    load_params.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
    web_contents_->GetController().LoadURLWithParams(load_params);

    // Apply default size boundaries, ensuring values are >= 1 to pass DCHECK.
    gfx::Size min_size_dip(1, 1);
    gfx::Size max_size_dip(INT_MAX, INT_MAX);

    // Respect optionally provided |min_size_dip|.
    if (params->min_size_dip.has_value())
      min_size_dip.SetToMax(params->min_size_dip.value());

    // Respect optionally provided |max_size_dip|.
    if (params->max_size_dip.has_value())
      max_size_dip.SetToMin(params->max_size_dip.value());

    // Enable auto-resizing.
    web_contents_->GetRenderWidgetHostView()->EnableAutoResize(min_size_dip,
                                                               max_size_dip);
  }

  void HandleWebContents(Profile* profile) {
    // When rendering WebContents in the same process as ash, we register the
    // associated view with the AnswerCardContentsRegistry's token-to-view map.
    // The token returned from the registry will uniquely identify the view.
    if (app_list::AnswerCardContentsRegistry::Get()) {
      web_view_ = std::make_unique<views::WebView>(profile);
      web_view_->set_owned_by_client();
      web_view_->SetResizeBackgroundColor(SK_ColorTRANSPARENT);
      web_view_->SetWebContents(web_contents_.get());

      embed_token_ = app_list::AnswerCardContentsRegistry::Get()->Register(
          web_view_.get(), web_contents_->GetNativeView());
    } else {
      // TODO(dmblack): Handle Mash case. https://crbug.com/854787.
    }
  }

  void HandleNavigationAttempt(const GURL& url,
                               WindowOpenDisposition disposition) {
    if (!open_url_delegate_)
      return;

    open_url_delegate_->ShouldOpenUrlFromTab(
        url, disposition,
        base::BindOnce(
            [](const base::WeakPtr<ManagedWebContents>& managed_web_contents,
               const GURL& url, bool should_open) {
              if (should_open && managed_web_contents)
                managed_web_contents->NavigateToUrl(url);
            },
            weak_factory_.GetWeakPtr(), url));
  }

  ash::mojom::WebContentsManager::ManageWebContentsCallback callback_;

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::WebView> web_view_;
  base::Optional<base::UnguessableToken> embed_token_;

  ash::mojom::ManagedWebContentsOpenUrlDelegatePtr open_url_delegate_;

  base::WeakPtrFactory<ManagedWebContents> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ManagedWebContents);
};

// WebContentsManager ----------------------------------------------------------

WebContentsManager::WebContentsManager(service_manager::Connector* connector)
    : binding_(this) {
  // Bind to the Assistant controller in ash.
  ash::mojom::AssistantControllerPtr assistant_controller;
  connector->BindInterface(ash::mojom::kServiceName, &assistant_controller);
  ash::mojom::WebContentsManagerPtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  assistant_controller->SetWebContentsManager(std::move(ptr));
}

WebContentsManager::~WebContentsManager() = default;

void WebContentsManager::ManageWebContents(
    const base::UnguessableToken& id_token,
    ash::mojom::ManagedWebContentsParamsPtr params,
    ash::mojom::WebContentsManager::ManageWebContentsCallback callback) {
  DCHECK(managed_web_contents_map_.count(id_token) == 0);
  managed_web_contents_map_[id_token] = std::make_unique<ManagedWebContents>(
      std::move(params), std::move(callback));
}

void WebContentsManager::ReleaseWebContents(
    const base::UnguessableToken& id_token) {
  managed_web_contents_map_.erase(id_token);
}

void WebContentsManager::ReleaseAllWebContents(
    const std::vector<base::UnguessableToken>& id_tokens) {
  for (const base::UnguessableToken& id_token : id_tokens)
    managed_web_contents_map_.erase(id_token);
}

void WebContentsManager::NavigateWebContentsBack(
    const base::UnguessableToken& id_token,
    ash::mojom::WebContentsManager::NavigateWebContentsBackCallback callback) {
  managed_web_contents_map_[id_token]->NavigateBack(std::move(callback));
}
