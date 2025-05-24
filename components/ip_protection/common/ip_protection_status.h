// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_STATUS_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_STATUS_H_

#include "base/observer_list.h"
#include "components/ip_protection/common/ip_protection_status_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-forward.h"

namespace content {
class RenderFrameHost;
class WebContents;
struct GlobalRequestID;
}  // namespace content

namespace ip_protection {

class IpProtectionStatusObserver;

// IpProtectionStatus is used to help determine the status of IP Protection on a
// given top-level frame.
class IpProtectionStatus
    : public content::WebContentsUserData<IpProtectionStatus>,
      public content::WebContentsObserver {
 public:
  // Creates an IpProtectionStatus object and attaches it to `web_contents`.
  //
  // If an IpProtectionStatus object already exists for the given
  // `web_contents`, this function does nothing.  If the IP Protection
  // feature (kEnableIpProtectionProxy) is disabled, this function also
  // does nothing.
  //
  // Other components should obtain a pointer to the IpProtectionStatus
  // instance (if one exists) using
  // `IpProtectionStatus::FromWebContents(web_contents)`.
  static void CreateForWebContents(content::WebContents* web_contents);

  IpProtectionStatus(const IpProtectionStatus&) = delete;
  IpProtectionStatus& operator=(const IpProtectionStatus&) = delete;
  ~IpProtectionStatus() override;

  // Adding and removing observers required for `base::ScopedObservation`.
  void AddObserver(IpProtectionStatusObserver* observer);
  void RemoveObserver(IpProtectionStatusObserver* observer);

  // Returns true if IPP is actively proxying subresources on the current
  // primary page.
  bool IsSubresourceProxiedOnCurrentPrimaryPage() const {
    return is_subresource_proxied_on_current_primary_page_;
  }

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void ResourceLoadComplete(
      content::RenderFrameHost* render_frame_host,
      const content::GlobalRequestID& request_id,
      const blink::mojom::ResourceLoadInfo& resource_load_info) override;

 private:
  friend class content::WebContentsUserData<IpProtectionStatus>;

  explicit IpProtectionStatus(content::WebContents* web_contents);

  // True if IP Protection is actively proxying subresources on the current
  // primary page and false otherwise. This state is reset on every primary
  // page change.
  bool is_subresource_proxied_on_current_primary_page_ = false;

  base::ObserverList<IpProtectionStatusObserver> observer_list_;

  // Data key required for WebContentsUserData.
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_STATUS_H_
