// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_PARSE_MANIFEST_FROM_STRING_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_PARSE_MANIFEST_FROM_STRING_JOB_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/model/parse_manifest_result.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom.h"
#include "url/gurl.h"

namespace base {
class DictValue;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

class WebContentsManager;

// Loads about:blank on `web_contents`, binds a ManifestManager mojo remote,
// parses `manifest_contents` as a web app manifest, and validates that the
// result has the required fields (valid start_url and name/short_name).
//
// Returns the parsed manifest or a structured failure reason.
//
// `document_url` and `manifest_url` are forwarded to ManifestParser for
// relative URL resolution. They may be the same URL when there is no
// associated document.
class ParseManifestFromStringJob {
 public:
  using ResultCallback =
      base::OnceCallback<void(ParseManifestResult parse_result)>;

  ParseManifestFromStringJob(WebContentsManager& web_contents_manager,
                             content::WebContents& web_contents,
                             GURL document_url,
                             GURL manifest_url,
                             std::string manifest_contents,
                             base::DictValue& debug_value,
                             ResultCallback callback);

  ~ParseManifestFromStringJob();

  void Start();

 private:
  void OnAboutBlankLoaded(webapps::WebAppUrlLoaderResult result);
  void OnManifestParsed(blink::mojom::ManifestPtr manifest);
  void OnManifestManagerDisconnected();

  const raw_ref<content::WebContents> web_contents_;
  GURL document_url_;
  GURL manifest_url_;
  std::string manifest_contents_;
  const raw_ref<base::DictValue> debug_value_;
  ResultCallback callback_;

  std::unique_ptr<webapps::WebAppUrlLoader> url_loader_;
  mojo::Remote<blink::mojom::ManifestManager> manifest_manager_;

  base::WeakPtrFactory<ParseManifestFromStringJob> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_PARSE_MANIFEST_FROM_STRING_JOB_H_
