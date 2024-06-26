// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/chrome_popup_navigation_delegate.h"

#include "build/build_config.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/blocked_content/popup_navigation_delegate.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/blocked_content/android/popup_blocked_message_delegate.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/messages_feature.h"
#endif

ChromePopupNavigationDelegate::ChromePopupNavigationDelegate(
    NavigateParams params)
    : params_(std::move(params)),
      original_user_gesture_(params_.user_gesture) {}

content::RenderFrameHost* ChromePopupNavigationDelegate::GetOpener() {
  return params_.opener;
}

bool ChromePopupNavigationDelegate::GetOriginalUserGesture() {
  return original_user_gesture_;
}

GURL ChromePopupNavigationDelegate::GetURL() {
  return params_.url;
}

blocked_content::PopupNavigationDelegate::NavigateResult
ChromePopupNavigationDelegate::NavigateWithGesture(
    const blink::mojom::WindowFeatures& window_features,
    std::optional<WindowOpenDisposition> updated_disposition) {
  params_.user_gesture = true;
  if (updated_disposition)
    params_.disposition = updated_disposition.value();
#if BUILDFLAG(IS_ANDROID)
  TabModelList::HandlePopupNavigation(&params_);
#else
  ::Navigate(&params_);
#endif
  if (params_.navigated_or_inserted_contents &&
      params_.disposition == WindowOpenDisposition::NEW_POPUP) {
    content::RenderFrameHost* host =
        params_.navigated_or_inserted_contents->GetPrimaryMainFrame();
    DCHECK(host);
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> client;
    host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
    client->SetWindowFeatures(window_features.Clone());
  }
  return NavigateResult{params_.navigated_or_inserted_contents,
                        params_.disposition};
}

void ChromePopupNavigationDelegate::OnPopupBlocked(
    content::WebContents* web_contents,
    int total_popups_blocked_on_page) {
#if BUILDFLAG(IS_ANDROID)
  bool is_created = false;
  messages::MessageDispatcherBridge* message_dispatcher_bridge =
      messages::MessageDispatcherBridge::Get();

  // It is possible that an initial navigation results in a blocked popup
  // before the //chrome-level initialization of the messages infrastructure
  // has run. Short-circuit out in that case to prevent a crash when
  // PopupBlockedMessageDelegate tries to map the resource ID via
  // MessageDispatcherBridge. crbug.com/1308214
  if (message_dispatcher_bridge->IsMessagesEnabledForEmbedder()) {
    blocked_content::PopupBlockedMessageDelegate::CreateForWebContents(
        web_contents);
    blocked_content::PopupBlockedMessageDelegate*
        popup_blocked_message_delegate =
            blocked_content::PopupBlockedMessageDelegate::FromWebContents(
                web_contents);
    is_created = popup_blocked_message_delegate->ShowMessage(
        total_popups_blocked_on_page,
        HostContentSettingsMapFactory::GetForProfile(
            web_contents->GetBrowserContext()),
        base::BindOnce(
            &content_settings::RecordPopupsAction,
            content_settings::POPUPS_ACTION_CLICKED_ALWAYS_SHOW_ON_MOBILE));
  }

  if (is_created) {
    content_settings::RecordPopupsAction(
        content_settings::POPUPS_ACTION_DISPLAYED_INFOBAR_ON_MOBILE);
  }
#endif
}
