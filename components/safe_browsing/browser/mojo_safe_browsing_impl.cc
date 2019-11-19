// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/browser/mojo_safe_browsing_impl.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/supports_user_data.h"
#include "components/safe_browsing/browser/safe_browsing_url_checker_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/resource_type.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/load_flags.h"

namespace safe_browsing {
namespace {

content::WebContents* GetWebContentsFromID(int render_process_id,
                                           int render_frame_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return nullptr;

  return content::WebContents::FromRenderFrameHost(render_frame_host);
}

// This class wraps a callback for checking URL, and runs it on destruction,
// if it hasn't been run yet.
class CheckUrlCallbackWrapper {
 public:
  using Callback = base::OnceCallback<
      void(mojo::PendingReceiver<mojom::UrlCheckNotifier>, bool, bool)>;

  explicit CheckUrlCallbackWrapper(Callback callback)
      : callback_(std::move(callback)) {}
  ~CheckUrlCallbackWrapper() {
    if (callback_)
      Run(mojo::NullReceiver(), true, false);
  }

  void Run(mojo::PendingReceiver<mojom::UrlCheckNotifier> slow_check_notifier,
           bool proceed,
           bool showed_interstitial) {
    std::move(callback_).Run(std::move(slow_check_notifier), proceed,
                             showed_interstitial);
  }

 private:
  Callback callback_;
};

// UserData object that owns MojoSafeBrowsingImpl. This is used rather than
// having MojoSafeBrowsingImpl directly extend base::SupportsUserData::Data to
// avoid naming conflicts between Data::Clone() and
// mojom::SafeBrowsing::Clone().
class SafeBrowserUserData : public base::SupportsUserData::Data {
 public:
  explicit SafeBrowserUserData(std::unique_ptr<MojoSafeBrowsingImpl> impl)
      : impl_(std::move(impl)) {}
  ~SafeBrowserUserData() override = default;

 private:
  std::unique_ptr<MojoSafeBrowsingImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowserUserData);
};

}  // namespace

MojoSafeBrowsingImpl::MojoSafeBrowsingImpl(
    scoped_refptr<UrlCheckerDelegate> delegate,
    int render_process_id,
    content::ResourceContext* resource_context)
    : delegate_(std::move(delegate)),
      render_process_id_(render_process_id),
      resource_context_(resource_context) {
  DCHECK(resource_context_);

  // It is safe to bind |this| as Unretained because |receivers_| is owned by
  // |this| and will not call this callback after it is destroyed.
  receivers_.set_disconnect_handler(base::Bind(
      &MojoSafeBrowsingImpl::OnMojoDisconnect, base::Unretained(this)));
}

MojoSafeBrowsingImpl::~MojoSafeBrowsingImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

// static
void MojoSafeBrowsingImpl::MaybeCreate(
    int render_process_id,
    content::ResourceContext* resource_context,
    const base::Callback<scoped_refptr<UrlCheckerDelegate>()>& delegate_getter,
    mojo::PendingReceiver<mojom::SafeBrowsing> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  scoped_refptr<UrlCheckerDelegate> delegate = delegate_getter.Run();

  if (!resource_context || !delegate ||
      !delegate->GetDatabaseManager()->IsSupported())
    return;

  std::unique_ptr<MojoSafeBrowsingImpl> impl(new MojoSafeBrowsingImpl(
      std::move(delegate), render_process_id, resource_context));
  impl->Clone(std::move(receiver));

  MojoSafeBrowsingImpl* raw_impl = impl.get();
  std::unique_ptr<SafeBrowserUserData> user_data =
      std::make_unique<SafeBrowserUserData>(std::move(impl));
  raw_impl->user_data_key_ = user_data.get();
  resource_context->SetUserData(raw_impl->user_data_key_, std::move(user_data));
}

void MojoSafeBrowsingImpl::CreateCheckerAndCheck(
    int32_t render_frame_id,
    mojo::PendingReceiver<mojom::SafeBrowsingUrlChecker> receiver,
    const GURL& url,
    const std::string& method,
    const net::HttpRequestHeaders& headers,
    int32_t load_flags,
    content::ResourceType resource_type,
    bool has_user_gesture,
    bool originated_from_service_worker,
    CreateCheckerAndCheckCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (delegate_->ShouldSkipRequestCheck(resource_context_, url,
                                        -1 /* frame_tree_node_id */,
                                        render_process_id_, render_frame_id,
                                        originated_from_service_worker)) {
    // Ensure that we don't destroy an uncalled CreateCheckerAndCheckCallback
    if (callback) {
      std::move(callback).Run(mojo::NullReceiver(), true /* proceed */,
                              false /* showed_interstitial */);
    }

    // This will drop |receiver|. The result is that the renderer side will
    // consider all URLs in the redirect chain of this receiver as safe.
    return;
  }

  // This is not called for frame resources, and real time URL checks currently
  // only support frame resources. If we extend real time URL checks to support
  // non-main frames, we will need to provide the user preferences regarding
  // real time lookup here.
  auto checker_impl = std::make_unique<SafeBrowsingUrlCheckerImpl>(
      headers, static_cast<int>(load_flags), resource_type, has_user_gesture,
      delegate_,
      base::Bind(&GetWebContentsFromID, render_process_id_,
                 static_cast<int>(render_frame_id)),
      /*real_time_lookup_enabled=*/false);

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
    resource_context_->RemoveUserData(user_data_key_);
    // This object is destroyed.
  }
}

}  // namespace safe_browsing
