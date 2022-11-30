// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webui/cast_webui.h"

namespace chromecast {

// static
std::unique_ptr<CastWebUI> CastWebUI::Create(content::WebUI* webui,
                                             const std::string& host,
                                             mojom::WebUiClient* client) {
  return std::make_unique<CastWebUI>(webui, host, client);
}

}  // namespace chromecast
