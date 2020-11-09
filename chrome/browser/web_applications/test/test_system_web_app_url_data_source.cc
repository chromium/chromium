// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_system_web_app_url_data_source.h"

#include <utility>

#include "base/memory/ref_counted_memory.h"
#include "base/test/bind_test_util.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "content/public/browser/web_ui_data_source.h"

namespace web_app {

namespace {

constexpr char kPwaHtml[] =
    R"(
<html>
<head>
  <link rel="manifest" href="manifest.json">
  <script>
    navigator.serviceWorker.register('sw.js');
  </script>
</head>
</html>
)";

constexpr char kPage2Html[] =
    R"(
<!DOCTYPE html><title>Page 2</title>
  )";

constexpr char kSwJs[] = "globalThis.addEventListener('fetch', event => {});";

}  // namespace

void AddTestURLDataSource(const std::string& source_name,
                          content::BrowserContext* browser_context) {
  static std::string manifest(kSystemAppManifestText);
  AddTestURLDataSource(source_name, &manifest, browser_context);
}

void AddTestURLDataSource(const std::string& source_name,
                          const std::string* manifest_text,
                          content::BrowserContext* browser_context) {
  content::WebUIDataSource* data_source =
      content::WebUIDataSource::Create(source_name);
  data_source->DisableTrustedTypesCSP();
  data_source->AddResourcePath("icon-256.png", IDR_PRODUCT_LOGO_256);
  data_source->SetRequestFilter(
      base::BindLambdaForTesting([](const std::string& path) {
        return path == "manifest.json" || path == "pwa.html" ||
               path == "page2.html";
      }),
      base::BindLambdaForTesting(
          [manifest_text](const std::string& id,
                          content::WebUIDataSource::GotDataCallback callback) {
            scoped_refptr<base::RefCountedString> ref_contents(
                new base::RefCountedString);
            if (id == "manifest.json")
              ref_contents->data() = *manifest_text;
            else if (id == "pwa.html")
              ref_contents->data() = kPwaHtml;
            else if (id == "sw.js")
              ref_contents->data() = kSwJs;
            else if (id == "page2.html")
              ref_contents->data() = kPage2Html;
            else
              NOTREACHED();

            std::move(callback).Run(ref_contents);
          }));
  content::WebUIDataSource::Add(browser_context, data_source);
}

}  // namespace web_app
