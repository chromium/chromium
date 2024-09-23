// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_MOJO_SAFE_BROWSING_IMPL_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_MOJO_SAFE_BROWSING_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/supports_user_data.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "components/safe_browsing/core/common/safe_browsing_url_checker.mojom.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/fetch_api.mojom.h"

namespace safe_browsing {

// This class implements the Mojo interface for renderers to perform
// SafeBrowsing URL checks.
// A MojoSafeBrowsingImpl instance is destructed when the Mojo message pipe is
// disconnected or |resource_context_| is destructed.
class MojoSafeBrowsingImpl : public mojom::SafeBrowsing,
                             public base::SupportsUserData::Data {
 public:
  MojoSafeBrowsingImpl(const MojoSafeBrowsingImpl&) = delete;
  MojoSafeBrowsingImpl& operator=(const MojoSafeBrowsingImpl&) = delete;

  ~MojoSafeBrowsingImpl() override;

  static void MaybeCreate(
      int render_process_id,
      const base::RepeatingCallback<scoped_refptr<UrlCheckerDelegate>()>&
          delegate_getter,
      mojo::PendingReceiver<mojom::SafeBrowsing> receiver);

 private:
  // Needed since there's a Clone method in two parent classes.
  using base::SupportsUserData::Data::Clone;

  MojoSafeBrowsingImpl(scoped_refptr<UrlCheckerDelegate> delegate,
                       int render_process_id,
                       base::SupportsUserData* user_data);

  // mojom::SafeBrowsing implementation.
  void CreateCheckerAndCheck(
      const std::optional<blink::LocalFrameToken>& frame_token,
      mojo::PendingReceiver<mojom::SafeBrowsingUrlChecker> receiver,
      const GURL& url,
      const std::string& method,
      const net::HttpRequestHeaders& headers,
      int32_t load_flags,
      bool has_user_gesture,
      bool originated_from_service_worker,
      CreateCheckerAndCheckCallback callback) override;
  void Clone(mojo::PendingReceiver<mojom::SafeBrowsing> receiver) override;

  void OnMojoDisconnect();

  // This is an instance of SafeBrowserUserData that is set as user-data on
  // |user_data_|. SafeBrowserUserData owns |this|.
  raw_ptr<const void> user_data_key_ = nullptr;

  mojo::ReceiverSet<mojom::SafeBrowsing> receivers_;
  scoped_refptr<UrlCheckerDelegate> delegate_;
  int render_process_id_ = MSG_ROUTING_NONE;

  // Guaranteed to outlive this object as it owns it.
  raw_ptr<base::SupportsUserData> user_data_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_MOJO_SAFE_BROWSING_IMPL_H_
