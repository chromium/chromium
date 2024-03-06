// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_EVALUATOR_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_EVALUATOR_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_page_data.h"
#include "components/webapps/browser/installable/installable_params.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

namespace webapps {

// This class is responsible for evaluating whether the page data is sufficient
// for installing the web app.
class InstallableEvaluator {
 public:
  // Gets the installable error on the display fields for the manifest. This is
  // public so this can be checked in FetchManifestAndInstallCommand after using
  // the `kValidManifestIgnoreDisplay` criteria.
  static InstallableStatusCode GetDisplayError(
      const blink::mojom::Manifest& manifest,
      InstallableCriteria criteria);

  InstallableEvaluator(content::WebContents* web_contents,
                       const InstallablePageData& data,
                       InstallableCriteria criteria);
  ~InstallableEvaluator();

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
  std::vector<InstallableStatusCode> CheckEligibility(
      content::WebContents*) const;

  // Check if the web site has provided all information required for install,
  // returns nullopt if the check was not run.
  std::optional<std::vector<InstallableStatusCode>> CheckInstallability() const;

 private:
  friend class InstallableEvaluatorUnitTest;
  friend class TestInstallableManager;

  base::WeakPtr<content::WebContents> web_contents_;
  const raw_ref<const InstallablePageData> page_data_;
  InstallableCriteria criteria_;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_EVALUATOR_H_
