// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/font_prewarmer.h"

#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/web_font_prewarmer.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/win/web_font_rendering.h"

// static
void FontPrewarmer::Bind(
    mojo::PendingReceiver<chrome::mojom::FontPrewarmer> pending_receiver) {
  new FontPrewarmer(std::move(pending_receiver));
}

void FontPrewarmer::PrewarmFonts(const std::vector<std::string>& font_names) {
  blink::WebFontPrewarmer* prewarmer =
      blink::WebFontRendering::GetFontPrewarmer();
  // `prewarmer` is not always present, such as in --single-process.
  if (!prewarmer)
    return;

  for (const std::string& font_name : font_names) {
    prewarmer->PrewarmFamily(blink::WebString::FromUTF8(font_name));
  }
}

FontPrewarmer::FontPrewarmer(
    mojo::PendingReceiver<chrome::mojom::FontPrewarmer> pending_receiver)
    : receiver_(this, std::move(pending_receiver)) {
  receiver_.set_disconnect_handler(
      base::BindOnce(&FontPrewarmer::OnDisconnect, base::Unretained(this)));
}

FontPrewarmer::~FontPrewarmer() = default;

void FontPrewarmer::OnDisconnect() {
  delete this;
}
