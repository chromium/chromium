// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_MOJO_SAFE_BROWSING_IMPL_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_MOJO_SAFE_BROWSING_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/supports_user_data.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

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

  explicit MojoSafeBrowsingImpl(base::SupportsUserData* user_data);

  // mojom::SafeBrowsing implementation.
  void Clone(mojo::PendingReceiver<mojom::SafeBrowsing> receiver) override;

  void OnMojoDisconnect();

  mojo::ReceiverSet<mojom::SafeBrowsing> receivers_;

  // Guaranteed to outlive this object as it owns it.
  raw_ptr<base::SupportsUserData> user_data_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_MOJO_SAFE_BROWSING_IMPL_H_
