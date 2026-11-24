// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BIDI_MAPPER_HOST_BIDI_MAPPER_UI_H_
#define COMPONENTS_BIDI_MAPPER_HOST_BIDI_MAPPER_UI_H_

#include "content/public/browser/webui_config.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace content {
class BrowserContext;
class WebUI;
}  // namespace content

namespace bidi_mapper {

class BidiMapperUI;

// The document that a BidiMapperHostImpl runs the BiDi mapper in.
//
// The document is empty: once the host has added its bindings, it loads the
// message filter and the mapper from this data source. It has an origin of its
// own, so that the mapper runs in a renderer process that is locked to that
// origin, and that web content and extensions cannot script or debug.
//
// The document is in the chrome-untrusted: scheme, and not in chrome:, because
// the mapper processes untrustworthy content: the BiDi commands of its client,
// and data from the pages that it automates. Such a document gets none of the
// capabilities that chrome: documents get by default, see
// docs/webui/chrome_untrusted.md. The DevTools protocol connection that the
// mapper needs is granted by the host, see BidiMapperHostImpl.
inline constexpr char kBidiMapperUIHost[] = "bidi-mapper";
inline constexpr char kBidiMapperUIURL[] = "chrome-untrusted://bidi-mapper/";
inline constexpr char kBidiMessageFilterScriptPath[] = "bidi_message_filter.js";
inline constexpr char kBidiMapperScriptPath[] = "bidi_mapper.js";

// The document only exists when BidiMapperHost::IsEnabled().
class BidiMapperUIConfig : public content::DefaultWebUIConfig<BidiMapperUI> {
 public:
  BidiMapperUIConfig();
  ~BidiMapperUIConfig() override;

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class BidiMapperUI : public ui::UntrustedWebUIController {
 public:
  explicit BidiMapperUI(content::WebUI* web_ui);
  BidiMapperUI(const BidiMapperUI&) = delete;
  BidiMapperUI& operator=(const BidiMapperUI&) = delete;
  ~BidiMapperUI() override;
};

}  // namespace bidi_mapper

#endif  // COMPONENTS_BIDI_MAPPER_HOST_BIDI_MAPPER_UI_H_
