// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_password_manager_driver.h"

#include "components/password_manager/core/browser/password_manager_util.h"
#include "content/public/browser/render_frame_host.h"

namespace password_manager {

namespace {

GURL StripAuth(const GURL& gurl) {
  GURL::Replacements rep;
  rep.ClearUsername();
  rep.ClearPassword();
  return gurl.ReplaceComponents(rep);
}

}  // namespace

GURL GetURLFromRenderFrameHost(content::RenderFrameHost* render_frame_host) {
  GURL url = render_frame_host->GetLastCommittedURL();
  // GetLastCommittedURL doesn't include URL updates due to document.open() and
  // so it might be about:blank or about:srcdoc. In this case fallback to
  // GetLastCommittedOrigin. Otherwise renderer process will be killed because
  // Password Manager can't use about:URLs to save passwords, see
  // http://crbug.com/1220333 for more details.
  if (url.SchemeIs(url::kAboutScheme)) {
    url = render_frame_host->GetLastCommittedOrigin().GetURL();
  }
  return url;
}

void SetFrameAndFormMetaData(content::RenderFrameHost* rfh,
                             autofill::FormData& form) {
  GURL url = GetURLFromRenderFrameHost(rfh);
  DCHECK(url.is_valid());
  form.set_host_frame(autofill::LocalFrameToken(rfh->GetFrameToken().value()));
  form.set_url(password_manager_util::StripAuthAndParams(url));
  form.set_full_url(StripAuth(url));
  form.set_main_frame_origin(rfh->GetMainFrame()->GetLastCommittedOrigin());
}

autofill::FormData GetFormWithFrameAndFormMetaData(
    content::RenderFrameHost* rfh,
    autofill::FormData form) {
  SetFrameAndFormMetaData(rfh, form);
  return form;
}

}  // namespace password_manager
