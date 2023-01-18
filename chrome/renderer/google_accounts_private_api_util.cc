// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/google_accounts_private_api_util.h"

#include "content/public/renderer/render_frame.h"
#include "google_apis/gaia/gaia_urls.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/origin.h"

namespace {

const url::Origin& GetAllowedGoogleAccountsOrigin() {
  const url::Origin& origin = GaiaUrls::GetInstance()->gaia_origin();
  CHECK(!origin.opaque());
  return origin;
}

}  // namespace

bool ShouldExposeGoogleAccountsJavascriptApi(
    content::RenderFrame* render_frame) {
  DCHECK(render_frame);

  const url::Origin origin = render_frame->GetWebFrame()->GetSecurityOrigin();
  return origin == GetAllowedGoogleAccountsOrigin() &&
         blink::Platform::Current()->IsLockedToSite();
}
