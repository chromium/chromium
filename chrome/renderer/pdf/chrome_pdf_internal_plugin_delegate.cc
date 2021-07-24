// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pdf/chrome_pdf_internal_plugin_delegate.h"

#include "chrome/common/pdf_util.h"
#include "chrome/common/webui_url_constants.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_frame.h"
#include "url/gurl.h"
#include "url/origin.h"

ChromePdfInternalPluginDelegate::ChromePdfInternalPluginDelegate() = default;

ChromePdfInternalPluginDelegate::~ChromePdfInternalPluginDelegate() = default;

bool ChromePdfInternalPluginDelegate::IsAllowedFrame(
    const blink::WebFrame& frame) const {
  // The in-process plugin should only be created if the parent frame has an
  // allowed origin.
  const blink::WebFrame* parent_frame = frame.Parent();
  if (!parent_frame) {
    // TODO(crbug.com/1225756): Until this is fixed, allow Print Preview to
    // create the in-process plugin directly within its own frames.
    return frame.GetSecurityOrigin().IsSameOriginWith(
        blink::WebSecurityOrigin::Create(GURL(chrome::kChromeUIPrintURL)));
  }

  return IsPdfInternalPluginAllowedOrigin(parent_frame->GetSecurityOrigin());
}
