// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_SOURCE_H_

#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"

namespace ash {

// Provides the web (html / js / css) content for mako
// This content is provided by ChromeOS in the rootfs at
// /usr/share/chromeos-assets/mako
class MakoSource : public content::URLDataSource {
 public:
  MakoSource() noexcept;

  // content::URLDataSource:
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const GURL& url) override;
  std::string GetContentSecurityPolicy(
      network::mojom::CSPDirectiveName directive) override;
};
}  // namespace ash
#endif  // CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_SOURCE_H_
