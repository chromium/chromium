// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/renderer/no_state_prefetch_client.h"

#include "base/logging.h"
#include "components/no_state_prefetch/renderer/no_state_prefetch_helper.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace prerender {

NoStatePrefetchClient::NoStatePrefetchClient(blink::WebView* web_view)
    : blink::WebViewObserver(web_view) {
  DCHECK(web_view);
  DVLOG(5) << "NoStatePrefetchClient::NoStatePrefetchClient()";
  web_view->SetNoStatePrefetchClient(this);
}

NoStatePrefetchClient::~NoStatePrefetchClient() = default;

bool NoStatePrefetchClient::IsPrefetchOnly() {
  blink::WebFrame* main_frame = GetWebView()->MainFrame();
  if (!main_frame->IsWebLocalFrame())
    return false;
  return NoStatePrefetchHelper::IsPrefetching(
      content::RenderFrame::FromWebFrame(main_frame->ToWebLocalFrame()));
}

void NoStatePrefetchClient::OnDestruct() {
  delete this;
}

}  // namespace prerender
