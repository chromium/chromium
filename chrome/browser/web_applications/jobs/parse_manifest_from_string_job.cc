// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/parse_manifest_from_string_job.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/to_string.h"
#include "base/values.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "content/public/browser/page_manifest_manager.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/url_constants.h"

namespace web_app {

ParseManifestFromStringJob::ParseManifestFromStringJob(
    WebContentsManager& web_contents_manager,
    content::WebContents& web_contents,
    GURL document_url,
    GURL manifest_url,
    std::string manifest_contents,
    base::DictValue& debug_value,
    ResultCallback callback)
    : web_contents_(web_contents),
      document_url_(std::move(document_url)),
      manifest_url_(std::move(manifest_url)),
      manifest_contents_(std::move(manifest_contents)),
      debug_value_(debug_value),
      callback_(std::move(callback)) {
  url_loader_ = web_contents_manager.CreateUrlLoader();
}

ParseManifestFromStringJob::~ParseManifestFromStringJob() = default;

void ParseManifestFromStringJob::Start() {
  url_loader_->LoadUrl(
      GURL(url::kAboutBlankURL), &*web_contents_,
      webapps::WebAppUrlLoader::UrlComparison::kExact,
      base::BindOnce(&ParseManifestFromStringJob::OnAboutBlankLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ParseManifestFromStringJob::OnAboutBlankLoaded(
    webapps::WebAppUrlLoaderResult result) {
  if (result != webapps::WebAppUrlLoaderResult::kUrlLoaded) {
    debug_value_->Set("about_blank_error", base::ToString(result));
    std::move(callback_).Run(
        base::unexpected(ParseManifestError::kInternalError));
    return;
  }

  // The shared web contents must have been reset to about:blank before command
  // execution.
  CHECK_EQ(web_contents_->GetURL(), GURL(url::kAboutBlankURL));
  debug_value_->Set("about_blank_loaded", true);

  content::PageManifestManager* page_manifest_manager =
      content::PageManifestManager::GetOrCreate(
          web_contents_->GetPrimaryPage());
  page_manifest_manager->ParseManifestFromString(
      document_url_, manifest_url_, manifest_contents_,
      base::BindOnce(&ParseManifestFromStringJob::OnManifestParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ParseManifestFromStringJob::OnManifestParsed(
    blink::mojom::ManifestPtr manifest) {
  // Note that most errors during parsing (e.g. errors to do with parsing a
  // particular field) are silently ignored. As long as the manifest is valid
  // JSON and contains a valid start_url and name, installation will proceed.
  if (blink::IsEmptyManifest(manifest)) {
    debug_value_->Set("manifest_parse_error", "invalid_manifest");
    std::move(callback_).Run(
        base::unexpected(ParseManifestError::kEmptyOrInvalidManifest));
    return;
  }

  if (!manifest->has_valid_specified_start_url) {
    debug_value_->Set("manifest_parse_error", "invalid_start_url");
    std::move(callback_).Run(
        base::unexpected(ParseManifestError::kStartUrlInvalid));
    return;
  }

  if (!manifest->short_name.has_value() && !manifest->name.has_value()) {
    debug_value_->Set("manifest_parse_error", "missing_name");
    std::move(callback_).Run(
        base::unexpected(ParseManifestError::kManifestMissingNameOrShortName));
    return;
  }

  debug_value_->Set("manifest_parsed", true);
  std::move(callback_).Run(std::move(manifest));
}

}  // namespace web_app
