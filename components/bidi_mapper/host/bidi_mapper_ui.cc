// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bidi_mapper/host/bidi_mapper_ui.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "components/bidi_mapper/host/bidi_mapper_host.h"
#include "components/grit/components_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"

namespace bidi_mapper {

namespace {

// The message filter and the mapper cannot be included as `<script>` elements
// here: both grab the DevTools protocol bindings from `window` as soon as they
// execute, whereas BidiMapperHostImpl only attaches and installs those bindings
// once this document has committed and loaded. Instead, the host imports both
// scripts dynamically after setting up the bindings.
constexpr char kDocument[] =
    "<!DOCTYPE html><html><head><meta charset=\"utf-8\"></head>"
    "<body></body></html>";

// Serves the empty root document; the scripts are served as resources.
bool ShouldHandleRequest(const std::string& path) {
  return path.empty();
}

void HandleRequest(const std::string& path,
                   content::WebUIDataSource::GotDataCallback callback) {
  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(std::string(kDocument)));
}

}  // namespace

BidiMapperUIConfig::BidiMapperUIConfig()
    : DefaultWebUIConfig(content::kChromeUIUntrustedScheme, kBidiMapperUIHost) {
}

BidiMapperUIConfig::~BidiMapperUIConfig() = default;

bool BidiMapperUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return BidiMapperHost::IsEnabled();
}

BidiMapperUI::BidiMapperUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kBidiMapperUIURL);
  source->SetRequestFilter(base::BindRepeating(&ShouldHandleRequest),
                           base::BindRepeating(&HandleRequest));
  source->AddResourcePath(kBidiMessageFilterScriptPath,
                          IDR_BIDI_MESSAGE_FILTER_JS);
  source->AddResourcePath(kBidiMapperScriptPath, IDR_CHROME_BIDI_MAPPER_JS);
}

BidiMapperUI::~BidiMapperUI() = default;

}  // namespace bidi_mapper
