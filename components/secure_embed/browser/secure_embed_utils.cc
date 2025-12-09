// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/secure_embed/browser/secure_embed_utils.h"

#include "content/public/browser/web_contents.h"

namespace secure_embed {

bool IsSecureEmbedGuestWebContents(content::WebContents* web_contents) {
  return web_contents ? !!web_contents->GetSecureEmbedConnector() : false;
}

}  // namespace secure_embed
