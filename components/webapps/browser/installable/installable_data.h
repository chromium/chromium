// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_DATA_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_DATA_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace webapps {

struct Screenshot {
  Screenshot(SkBitmap image, std::optional<std::u16string> label);
  Screenshot(const Screenshot&);
  Screenshot& operator=(const Screenshot&);

  ~Screenshot();

  SkBitmap image;

  // Label for accessibility.
  std::optional<std::u16string> label;
};

// This struct contains the results of an InstallableManager::GetData call and
// is passed to an InstallableCallback. Each pointer and reference is owned by
// InstallableManager, and callers should copy any objects which they wish to
// use later. Fields not requested in GetData may or may not be set.
struct InstallableData {
  InstallableData(std::vector<InstallableStatusCode> errors,
                  const GURL& manifest_url,
                  const blink::mojom::Manifest& manifest,
                  const mojom::WebPageMetadata& metadata,
                  const GURL& primary_icon_url,
                  const SkBitmap* primary_icon,
                  bool has_maskable_primary_icon,
                  const std::vector<Screenshot>& screenshots,
                  bool installable_check_passed);

  InstallableData(const InstallableData&) = delete;
  InstallableData& operator=(const InstallableData&) = delete;

  ~InstallableData();

  // Returns the first error if any one exist. Otherwise returns
  // NO_ERROR_DETECTED.
  InstallableStatusCode GetFirstError() const;

  // Contains all errors encountered during the InstallableManager::GetData
  // call. Empty if no errors were encountered.
  std::vector<InstallableStatusCode> errors;

  // The URL of the the web app manifest. Empty if the site has no
  // <link rel="manifest"> tag.
  const raw_ref<const GURL, DanglingUntriaged> manifest_url;

  // The parsed web app manifest.
  const raw_ref<const blink::mojom::Manifest, DanglingUntriaged> manifest;

  // Manifest data provided by the HTML document.
  const raw_ref<const mojom::WebPageMetadata, DanglingUntriaged>
      web_page_metadata;

  // The URL of the chosen primary icon.
  const raw_ref<const GURL, DanglingUntriaged> primary_icon_url;

  // nullptr if the most appropriate primary icon couldn't be determined or
  // downloaded. The underlying primary icon is owned by the InstallableManager;
  // clients must copy the bitmap if they want to to use it.
  raw_ptr<const SkBitmap, DanglingUntriaged> primary_icon;

  // Whether the primary icon had the 'maskable' purpose, meaningless if no
  // primary_icon was requested.
  const bool has_maskable_primary_icon;

  // The screenshots to show in the install UI.
  const raw_ref<const std::vector<Screenshot>, DanglingUntriaged> screenshots;

  // Whether the site has provided sufficient info for installing the web app.
  // i.e. a valid, installable web app manifest. The result might be different
  // depending on the task's |params|. If |installable_check_passed| was true
  // and the site isn't installable, the reason will be in |errors|.
  const bool installable_check_passed = false;
};

using InstallableCallback = base::OnceCallback<void(const InstallableData&)>;

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_DATA_H_
