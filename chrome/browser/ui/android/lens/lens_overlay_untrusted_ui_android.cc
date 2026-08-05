// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/lens/lens_overlay_untrusted_ui_android.h"

#include "base/memory/ref_counted_memory.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace lens {

namespace {

// TODO(crbug.com/526671849): These hardcoded strings are temporary placeholders
// and MUST be removed when the real WebUI assets (HTML/JS files) are ported and
// served via standard Chromium resource packing (.grd files).
constexpr char kLensOverlayCss[] = R"CSS(html, body {
  align-items: center;
  display: flex;
  font-family: sans-serif;
  height: 100vh;
  justify-content: center;
  margin: 0;
  padding: 0;
  width: 100vw;
})CSS";

constexpr char kLensOverlayJs[] =
    R"JS(document.body.addEventListener('click', () => {
  if (window.lensOverlay) {
    window.lensOverlay.close();
  }
});)JS";

constexpr char kLensOverlayHtml[] = R"HTML(<!DOCTYPE html>
<html>
<head>
  <link rel="stylesheet" href="lens_overlay.css">
</head>
<body role="dialog" aria-label="Lens Overlay Placeholder">
  <h1>Lens Overlay Placeholder: Click to dismiss.</h1>
  <script type="module" src="lens_overlay.js"></script>
</body>
</html>)HTML";

}  // namespace

/******** LensOverlayUntrustedUIAndroidConfig ********/

LensOverlayUntrustedUIAndroidConfig::LensOverlayUntrustedUIAndroidConfig()
    : content::WebUIConfig(content::kChromeUIUntrustedScheme,
                           chrome::kChromeUILensOverlayHost) {}

std::unique_ptr<content::WebUIController>
LensOverlayUntrustedUIAndroidConfig::CreateWebUIController(
    content::WebUI* web_ui,
    const GURL& url) {
  return std::make_unique<LensOverlayUntrustedUIAndroid>(web_ui);
}

/******** LensOverlayUntrustedUIAndroid ********/

LensOverlayUntrustedUIAndroid::LensOverlayUntrustedUIAndroid(
    content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUILensOverlayUntrustedURL);

  source->SetRequestFilter(
      base::BindRepeating([](const std::string& path) {
        return path.empty() || path == "lens_overlay.css" ||
               path == "lens_overlay.js";
      }),
      base::BindRepeating(
          [](const std::string& path,
             content::WebUIDataSource::GotDataCallback callback) {
            std::string content;
            if (path == "lens_overlay.css") {
              content = kLensOverlayCss;
            } else if (path == "lens_overlay.js") {
              content = kLensOverlayJs;
            } else {
              content = kLensOverlayHtml;
            }
            std::move(callback).Run(
                base::MakeRefCounted<base::RefCountedString>(
                    std::move(content)));
          }));
}

}  // namespace lens
