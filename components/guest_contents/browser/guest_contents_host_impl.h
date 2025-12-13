// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_CONTENTS_BROWSER_GUEST_CONTENTS_HOST_IMPL_H_
#define COMPONENTS_GUEST_CONTENTS_BROWSER_GUEST_CONTENTS_HOST_IMPL_H_

#include "base/values.h"
#include "components/guest_contents/browser/guest_contents_handle.h"
#include "components/guest_contents/common/guest_contents.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class WebContents;
}  // namespace content

namespace guest_contents {

// Implements the mojom::GuestContentsHost interface available in the browser
// process on a WebUIController.
class GuestContentsHostImpl : public mojom::GuestContentsHost,
                              public content::WebContentsObserver {
 public:
  // The binder function used by WebUIController to create an implementation of
  // mojom::GuestContentsHost for the outer WebContents.
  static void Create(
      content::WebContents* outer_web_contents,
      mojo::PendingReceiver<guest_contents::mojom::GuestContentsHost> receiver);

 private:
  explicit GuestContentsHostImpl(content::WebContents* outer_web_contents);

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

  // mojom::GuestContentsHost:
  void Attach(const blink::LocalFrameToken& token_of_frame_to_swap,
              GuestId guest_contents_id,
              AttachCallback callback) override;

  raw_ptr<content::WebContents> outer_web_contents_;
};

}  // namespace guest_contents

#endif  // COMPONENTS_GUEST_CONTENTS_BROWSER_GUEST_CONTENTS_HOST_IMPL_H_
