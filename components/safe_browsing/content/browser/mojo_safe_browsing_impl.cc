// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/mojo_safe_browsing_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/supports_user_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace safe_browsing {

MojoSafeBrowsingImpl::MojoSafeBrowsingImpl(base::SupportsUserData* user_data)
    : user_data_(user_data) {
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

  std::unique_ptr<MojoSafeBrowsingImpl> impl(
      new MojoSafeBrowsingImpl(user_data));
  impl->Clone(std::move(receiver));

  // Need to store the value of |impl.get()| in a temp variable instead of
  // getting the value on the same line as |std::move(impl)|, because the
  // evaluation order is unspecified.
  const void* key = impl.get();
  user_data->SetUserData(key, std::move(impl));
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
