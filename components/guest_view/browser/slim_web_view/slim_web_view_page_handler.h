// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_PAGE_HANDLER_H_
#define COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_PAGE_HANDLER_H_

#include "base/values.h"
#include "components/guest_view/browser/slim_web_view/slim_web_view.mojom.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace guest_view {

class GuestViewManager;

class SlimWebViewPageHandler
    : public content::DocumentUserData<SlimWebViewPageHandler>,
      public mojom::PageHandler {
 public:
  DOCUMENT_USER_DATA_KEY_DECL();

  SlimWebViewPageHandler(const SlimWebViewPageHandler&) = delete;
  SlimWebViewPageHandler& operator=(const SlimWebViewPageHandler&) = delete;

  ~SlimWebViewPageHandler() override;

  void DispatchEvent(const std::string& event_name,
                     base::DictValue args,
                     int instance_id);

  // mojom::PageHandler implementation.
  void CreateGuest(base::DictValue create_params,
                   CreateGuestCallback callback) override;
  void SetSize(int32_t guest_instance_id,
               mojom::SetSizeParamsPtr size_params) override;
  void Navigate(int32_t guest_instance_id, const GURL& url) override;
  void SetPermission(int32_t guest_instance_id,
                     int32_t request_id,
                     mojom::PageHandler_PermissionResponseAction action,
                     SetPermissionCallback callback) override;

 private:
  friend class content::DocumentUserData<SlimWebViewPageHandler>;
  SlimWebViewPageHandler(content::RenderFrameHost* render_frame_host,
                         mojo::PendingReceiver<PageHandler> page_handler,
                         mojo::PendingRemote<mojom::Page> page);

  GuestViewManager* GetGuestViewManager();

  mojo::Receiver<PageHandler> receiver_;
  mojo::Remote<mojom::Page> page_;
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_PAGE_HANDLER_H_
