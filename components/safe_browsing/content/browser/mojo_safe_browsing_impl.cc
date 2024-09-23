// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/mojo_safe_browsing_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/supports_user_data.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/load_flags.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace safe_browsing {
namespace {

content::WebContents* GetWebContentsFromToken(
    int render_process_id,
    const std::optional<blink::LocalFrameToken>& frame_token) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!frame_token) {
    return nullptr;
  }
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromFrameToken(
          content::GlobalRenderFrameHostToken(render_process_id,
                                              frame_token.value()));
  if (!render_frame_host) {
    return nullptr;
  }

  return content::WebContents::FromRenderFrameHost(render_frame_host);
}

}  // namespace

MojoSafeBrowsingImpl::MojoSafeBrowsingImpl(
    scoped_refptr<UrlCheckerDelegate> delegate,
    int render_process_id,
    base::SupportsUserData* user_data)
    : delegate_(std::move(delegate)),
      render_process_id_(render_process_id),
      user_data_(user_data) {
  // It is safe to bind |this| as Unretained because |receivers_| is owned by
  // |this| and will not call this callback after it is destroyed.
  receivers_.set_disconnect_handler(base::BindRepeating(
      &MojoSafeBrowsingImpl::OnMojoDisconnect, base::Unretained(this)));
}

MojoSafeBrowsingImpl::~MojoSafeBrowsingImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

// static
void MojoSafeBrowsingImpl::MaybeCreate(
    int render_process_id,
    const base::RepeatingCallback<scoped_refptr<UrlCheckerDelegate>()>&
        delegate_getter,
    mojo::PendingReceiver<mojom::SafeBrowsing> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  scoped_refptr<UrlCheckerDelegate> delegate = delegate_getter.Run();

  if (!delegate) {
    return;
  }

  base::SupportsUserData* user_data;
  content::RenderProcessHost* rph =
      content::RenderProcessHost::FromID(render_process_id);
  DCHECK(rph);
  user_data = rph->GetBrowserContext();

  std::unique_ptr<MojoSafeBrowsingImpl> impl(new MojoSafeBrowsingImpl(
      std::move(delegate), render_process_id, user_data));
  impl->Clone(std::move(receiver));

  // Need to store the value of |impl.get()| in a temp variable instead of
  // getting the value on the same line as |std::move(impl)|, because the
  // evaluation order is unspecified.
  const void* key = impl.get();
  user_data->SetUserData(key, std::move(impl));
}

void MojoSafeBrowsingImpl::CreateCheckerAndCheck(
    const std::optional<blink::LocalFrameToken>& frame_token,
    mojo::PendingReceiver<mojom::SafeBrowsingUrlChecker> receiver,
    const GURL& url,
    const std::string& method,
    const net::HttpRequestHeaders& headers,
    int32_t load_flags,
    bool has_user_gesture,
    bool originated_from_service_worker,
    CreateCheckerAndCheckCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::optional<base::UnguessableToken> sb_frame_token;
  if (frame_token) {
    sb_frame_token = frame_token->value();
  }
  if (delegate_->ShouldSkipRequestCheck(url, content::FrameTreeNodeId().value(),
                                        render_process_id_, sb_frame_token,
                                        originated_from_service_worker)) {
    // Ensure that we don't destroy an uncalled CreateCheckerAndCheckCallback
    if (callback) {
      std::move(callback).Run(true /* proceed */,
                              false /* showed_interstitial */);
    }

    // This will drop |receiver|. The result is that the renderer side will
    // consider all URLs in the redirect chain of this receiver as safe.
    return;
  }

  // This is not called for frame resources, and real time URL checks currently
  // only support main frame resources. If we extend real time URL checks to
  // support non-main frames, we will need to provide the user preferences,
  // url_lookup_service regarding real time lookup here. If we extend
  // hash-prefix real-time checks to support non-main frames, we will need to
  // provide the hash_realtime_service_on_ui here.
  auto checker_impl = std::make_unique<SafeBrowsingUrlCheckerImpl>(
      headers, static_cast<int>(load_flags), has_user_gesture, delegate_,
      base::BindRepeating(&GetWebContentsFromToken, render_process_id_,
                          frame_token),
      /*weak_web_state=*/nullptr, render_process_id_, sb_frame_token,
      content::FrameTreeNodeId().value(),
      /*navigation_id=*/std::nullopt,
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_db=*/true, /*can_check_high_confidence_allowlist=*/true,
      /*url_lookup_service_metric_suffix=*/".None",
      content::GetUIThreadTaskRunner({}),
      /*url_lookup_service=*/nullptr,
      /*hash_realtime_service_on_ui=*/nullptr,
      /*hash_realtime_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone,
      /*is_async_check=*/false, /*check_allowlist_before_hash_database=*/false,
      SessionID::InvalidValue());
  auto weak_impl = checker_impl->WeakPtr();

  checker_impl->CheckUrl(url, method,
                         mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                             std::move(callback),
                             /*proceed=*/true, /*showed_interstitial=*/false));
  CHECK(weak_impl);  // This is to ensure calling CheckUrl doesn't delete itself
  mojo::MakeSelfOwnedReceiver(std::move(checker_impl), std::move(receiver));
}

void MojoSafeBrowsingImpl::Clone(
    mojo::PendingReceiver<mojom::SafeBrowsing> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MojoSafeBrowsingImpl::OnMojoDisconnect() {
  if (receivers_.empty()) {
    user_data_->RemoveUserData(this);
    // This object is destroyed.
  }
}

}  // namespace safe_browsing
