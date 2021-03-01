// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/plugin_vm_internal/plugin_vm_internal_ui.h"

#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/url_data_source.h"

// TODO(b/173653141) We need to port this to lacros eventually.

namespace chromeos {

namespace {

class DataSource : public content::URLDataSource {
  std::string GetSource() override {
    return chrome::kChromeUIPluginVmInternalHost;
  }

  void StartDataRequest(const GURL& url,
                        const content::WebContents::Getter& wc_getter,
                        GotDataCallback callback) override {
    // TODO(b/173653141) Send actual diagnosis.
    std::string rv = "parallels diagnosis";
    std::move(callback).Run(base::RefCountedString::TakeString(&rv));
  }

  std::string GetMimeType(const std::string& path) override {
    return "text/plain";
  }

  bool AllowCaching() override {
    // We need to generate new diagnosis every time.
    return false;
  }
};

}  // namespace

PluginVmInternalUI::PluginVmInternalUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  auto* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(profile, std::make_unique<DataSource>());
}

PluginVmInternalUI::~PluginVmInternalUI() = default;

}  // namespace chromeos
