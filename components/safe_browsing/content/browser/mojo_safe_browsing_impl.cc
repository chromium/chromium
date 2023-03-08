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
#include "content/public/browser/resource_context.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/load_flags.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace safe_browsing {
namespace {

content::WebContents* GetWebContentsFromID(int render_process_id,
                                           int render_frame_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host) {
    return nullptr;
  }

  return content::WebContents::FromRenderFrameHost(render_frame_host);
}

// This class wraps a callback for checking URL, and runs it on destruction,
// if it hasn't been run yet.
class CheckUrlCallbackWrapper {
 public:
  using Callback =
      base::OnceCallback<void(mojo::PendingReceiver<mojom::UrlCheckNotifier>,
                              bool,
                              bool,
                              bool,
                              bool)>;

  explicit CheckUrlCallbackWrapper(Callback callback)
      : callback_(std::move(callback)) {}
  ~CheckUrlCallbackWrapper() {
    if (callback_) {
      Run(mojo::NullReceiver(), true, false, false, false);
    }
  }

  void Run(mojo::PendingReceiver<mojom::UrlCheckNotifier> slow_check_notifier,
           bool proceed,
           bool showed_interstitial,
           bool did_perform_real_time_check,
           bool did_check_allowlist) {
    std::move(callback_).Run(std::move(slow_check_notifier), proceed,
                             showed_interstitial, did_perform_real_time_check,
                             did_check_allowlist);
  }

 private:
  Callback callback_;
};

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
  DCHECK_CURRENTLY_ON(
      base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)
          ? content::BrowserThread::UI
          : content::BrowserThread::IO);
}

// static
void MojoSafeBrowsingImpl::MaybeCreate(
    int render_process_id,
    base::WeakPtr<content::ResourceContext> resource_context,
    const base::RepeatingCallback<scoped_refptr<UrlCheckerDelegate>()>&
        delegate_getter,
    mojo::PendingReceiver<mojom::SafeBrowsing> receiver) {
  DCHECK_CURRENTLY_ON(
      base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)
          ? content::BrowserThread::UI
          : content::BrowserThread::IO);

  scoped_refptr<UrlCheckerDelegate> delegate = delegate_getter.Run();

  if (!delegate) {
    return;
  }

  // MojoSafeBrowsingImpl is either a UserData on ResourceContext or
  // BrowserContext depending on which thread safe browsing runs on.
  base::SupportsUserData* user_data;
  if (base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)) {
    content::RenderProcessHost* rph =
        content::RenderProcessHost::FromID(render_process_id);
    DCHECK(rph);
    user_data = rph->GetBrowserContext();
  } else {
    if (!resource_context) {
      // The ResourceContext was deleted in between the hop from the UI thread
      // to the IO thread.
      return;
    }
    user_data = resource_context.get();
  }

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
    int32_t render_frame_id,
    mojo::PendingReceiver<mojom::SafeBrowsingUrlChecker> receiver,
    const GURL& url,
    const std::string& method,
    const net::HttpRequestHeaders& headers,
    int32_t load_flags,
    network::mojom::RequestDestination request_destination,
    bool has_user_gesture,
    bool originated_from_service_worker,
    CreateCheckerAndCheckCallback callback) {
  DCHECK_CURRENTLY_ON(
      base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)
          ? content::BrowserThread::UI
          : content::BrowserThread::IO);

  if (delegate_->ShouldSkipRequestCheck(
          url, content::RenderFrameHost::kNoFrameTreeNodeId, render_process_id_,
          render_frame_id, originated_from_service_worker)) {
    // Ensure that we don't destroy an uncalled CreateCheckerAndCheckCallback
    if (callback) {
      std::move(callback).Run(mojo::NullReceiver(), true /* proceed */,
                              false /* showed_interstitial */,
                              false /* did_perform_real_time_check */,
                              false /* did_check_allowlist */);
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
      headers, static_cast<int>(load_flags), request_destination,
      has_user_gesture, delegate_,
      base::BindRepeating(&GetWebContentsFromID, render_process_id_,
                          static_cast<int>(render_frame_id)),
      render_process_id_, render_frame_id,
      content::RenderFrameHost::kNoFrameTreeNodeId,
      /*real_time_lookup_enabled=*/false,
      /*can_rt_check_subresource_url=*/false,
      /*can_check_db=*/true, /*can_check_high_confidence_allowlist=*/true,
      /*url_lookup_service_metric_suffix=*/".None",
      /*last_committed_url=*/GURL(), content::GetUIThreadTaskRunner({}),
      /*url_lookup_service=*/nullptr, WebUIInfoSingleton::GetInstance(),
      /*hash_realtime_service_on_ui=*/nullptr,
      /*mechanism_experimenter=*/nullptr,
      /*is_mechanism_experiment_allowed=*/false);

  checker_impl->CheckUrl(
      url, method,
      base::BindOnce(
          &CheckUrlCallbackWrapper::Run,
          base::Owned(new CheckUrlCallbackWrapper(std::move(callback)))));
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
