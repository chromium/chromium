// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_status.h"

#include "base/feature_list.h"
#include "components/ip_protection/common/ip_protection_status_observer.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/features.h"
#include "net/base/proxy_chain.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"

namespace ip_protection {

// static
void IpProtectionStatus::CreateForWebContents(
    content::WebContents* web_contents) {
  CHECK(base::FeatureList::IsEnabled(net::features::kEnableIpProtectionProxy));

  content::WebContentsUserData<IpProtectionStatus>::CreateForWebContents(
      web_contents);
}

IpProtectionStatus::IpProtectionStatus(content::WebContents* web_contents)
    : content::WebContentsUserData<IpProtectionStatus>(*web_contents),
      content::WebContentsObserver(web_contents) {}

IpProtectionStatus::~IpProtectionStatus() = default;

void IpProtectionStatus::AddObserver(IpProtectionStatusObserver* observer) {
  observer_list_.AddObserver(observer);
}

void IpProtectionStatus::RemoveObserver(IpProtectionStatusObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

// content::WebContentsObserver:
void IpProtectionStatus::PrimaryPageChanged(content::Page& page) {
  is_subresource_proxied_on_current_primary_page_ = false;
}

void IpProtectionStatus::ResourceLoadComplete(
    content::RenderFrameHost* render_frame_host,
    const content::GlobalRequestID& request_id,
    const blink::mojom::ResourceLoadInfo& resource_load_info) {
  // If the subresource was already proxied on this page, return early.
  if (is_subresource_proxied_on_current_primary_page_) {
    return;
  }
  const net::ProxyChain& proxy_chain = resource_load_info.proxy_chain;

  if (proxy_chain.is_for_ip_protection() && !proxy_chain.is_direct()) {
    is_subresource_proxied_on_current_primary_page_ = true;

    // Notify observers that a subresource was proxied on the current page.
    for (const auto& observer : observer_list_) {
      observer.OnFirstSubresourceProxiedOnCurrentPrimaryPage();
    }
  }
}

// Data key required for WebContentsUserData.
WEB_CONTENTS_USER_DATA_KEY_IMPL(IpProtectionStatus);

}  // namespace ip_protection
