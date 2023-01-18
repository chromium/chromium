// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/google_accounts_private_api_extension.h"

#include "chrome/renderer/google_accounts_private_api_util.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "v8/include/v8-context.h"

// static
void GoogleAccountsPrivateApiExtension::Create(content::RenderFrame* frame) {
  new GoogleAccountsPrivateApiExtension(frame);
}

GoogleAccountsPrivateApiExtension::GoogleAccountsPrivateApiExtension(
    content::RenderFrame* frame)
    : content::RenderFrameObserver(frame) {}

GoogleAccountsPrivateApiExtension::~GoogleAccountsPrivateApiExtension() =
    default;

void GoogleAccountsPrivateApiExtension::OnDestruct() {
  delete this;
}

void GoogleAccountsPrivateApiExtension::DidCreateScriptContext(
    v8::Local<v8::Context> v8_context,
    int32_t world_id) {
  if (!render_frame() || world_id != content::ISOLATED_WORLD_ID_GLOBAL) {
    return;
  }

  if (ShouldExposeGoogleAccountsJavascriptApi(render_frame())) {
    InjectScript();
  }
}

void GoogleAccountsPrivateApiExtension::InjectScript() {
  DCHECK(render_frame());
  // To be implemented.
}
