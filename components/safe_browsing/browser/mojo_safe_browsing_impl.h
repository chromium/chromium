// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_BROWSER_MOJO_SAFE_BROWSING_IMPL_H_
#define COMPONENTS_SAFE_BROWSING_BROWSER_MOJO_SAFE_BROWSING_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/safe_browsing/browser/url_checker_delegate.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {
class ResourceContext;
}

namespace safe_browsing {

// This class implements the Mojo interface for renderers to perform
// SafeBrowsing URL checks.
// A MojoSafeBrowsingImpl instance is destructed when the Mojo message pipe is
// disconnected or |resource_context_| is destructed.
class MojoSafeBrowsingImpl : public mojom::SafeBrowsing {
 public:
  ~MojoSafeBrowsingImpl() override;

  static void MaybeCreate(
      int render_process_id,
      content::ResourceContext* resource_context,
      const base::Callback<scoped_refptr<UrlCheckerDelegate>()>&
          delegate_getter,
      mojo::PendingReceiver<mojom::SafeBrowsing> receiver);

 private:
  MojoSafeBrowsingImpl(scoped_refptr<UrlCheckerDelegate> delegate,
                       int render_process_id,
                       content::ResourceContext* resource_context);

  // mojom::SafeBrowsing implementation.
  void CreateCheckerAndCheck(
      int32_t render_frame_id,
      mojo::PendingReceiver<mojom::SafeBrowsingUrlChecker> receiver,
      const GURL& url,
      const std::string& method,
      const net::HttpRequestHeaders& headers,
      int32_t load_flags,
      content::ResourceType resource_type,
      bool has_user_gesture,
      bool originated_from_service_worker,
      CreateCheckerAndCheckCallback callback) override;
  void Clone(mojo::PendingReceiver<mojom::SafeBrowsing> receiver) override;

  void OnMojoDisconnect();

  // This is an instance of SafeBrowserUserData that is set as user-data on
  // |resource_context_|. SafeBrowserUserData owns |this|.
  const void* user_data_key_ = nullptr;

  mojo::ReceiverSet<mojom::SafeBrowsing> receivers_;
  scoped_refptr<UrlCheckerDelegate> delegate_;
  int render_process_id_ = MSG_ROUTING_NONE;

  // Not owned by this object. It is always valid during the lifetime of this
  // object.
  content::ResourceContext* resource_context_;

  DISALLOW_COPY_AND_ASSIGN(MojoSafeBrowsingImpl);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_BROWSER_MOJO_SAFE_BROWSING_IMPL_H_
