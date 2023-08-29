// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_EVALUATOR_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_EVALUATOR_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/webapps/browser/installable/installable_icon_fetcher.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_page_data.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

namespace webapps {

// This class is responsible for evaluating whether the page data is sufficient
// for installing the web app.
class InstallableEvaluator {
 public:
  InstallableEvaluator(const InstallablePageData& data, bool check_display);

  // Maximum dimension size in pixels for icons.
  static const int kMaximumIconSizeInPx =
#if BUILDFLAG(IS_ANDROID)
      std::numeric_limits<int>::max();
#else
      1024;
#endif

  // Returns the minimum icon size in pixels for a site to be installable.
  static int GetMinimumIconSizeInPx();

  // Returns true if the overall security state of |web_contents| is sufficient
  // to be considered installable.
  static bool IsContentSecure(content::WebContents* web_contents);

  // Returns true for localhost and URLs that have been explicitly marked as
  // secure via a flag.
  static bool IsOriginConsideredSecure(const GURL& url);

  // Check if the web content is an incognito window or insecure context.
  std::vector<InstallableStatusCode> CheckEligiblity(content::WebContents*);
  std::vector<InstallableStatusCode> CheckManifestValid();

 private:
  friend class InstallableEvaluatorUnitTest;
  friend class TestInstallableManager;

  static std::vector<InstallableStatusCode> IsManifestValidForWebApp(
      const blink::mojom::Manifest& manifest,
      bool check_webapp_manifest_display);

  const raw_ref<const InstallablePageData> page_data_;
  bool check_display_;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_EVALUATOR_H_
