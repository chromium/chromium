// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_DATA_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_DATA_H_

#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace webapps {

// This struct contains the results of an InstallableManager::GetData call and
// is passed to an InstallableCallback. Each pointer and reference is owned by
// InstallableManager, and callers should copy any objects which they wish to
// use later. Fields not requested in GetData may or may not be set.
struct InstallableData {
  InstallableData(std::vector<InstallableStatusCode> errors,
                  const GURL& manifest_url,
                  const blink::mojom::Manifest& manifest,
                  const GURL& primary_icon_url,
                  const SkBitmap* primary_icon,
                  bool has_maskable_primary_icon,
                  const GURL& splash_icon_url,
                  const SkBitmap* splash_icon,
                  bool has_maskable_splash_icon,
                  const std::vector<SkBitmap>& screenshots,
                  bool valid_manifest,
                  bool worker_check_passed);

  InstallableData(const InstallableData&) = delete;
  InstallableData& operator=(const InstallableData&) = delete;

  ~InstallableData();

  // Returns true if `errors` is empty or only has `WARN_NOT_OFFLINE_CAPABLE`.
  // `WARN_NOT_OFFLINE_CAPABLE` only logs a warning message in DevTools and
  // should not change the behavior.
  // TODO(https://crbug.com/965802): Remove `WARN_NOT_OFFLINE_CAPABLE` once the
  // CheckOfflineCapability feature is enabled with 'enforce' mode by default in
  // M93.
  bool NoBlockingErrors() const;

  // Returns true if there is any |errors| and all errors are service worker
  // errors, i.e.|NO_MATCHING_SERVICE_WORKER| or |NOT_OFFLINE_CAPABLE|.
  bool HasErrorOnlyServiceWorkerErrors() const;

  // Contains all errors encountered during the InstallableManager::GetData
  // call. Empty if no errors were encountered.
  std::vector<InstallableStatusCode> errors;

  // The URL of the the web app manifest. Empty if the site has no
  // <link rel="manifest"> tag.
  const GURL& manifest_url;

  // The parsed web app manifest.
  const blink::mojom::Manifest& manifest;

  // The URL of the chosen primary icon.
  const GURL& primary_icon_url;

  // nullptr if the most appropriate primary icon couldn't be determined or
  // downloaded. The underlying primary icon is owned by the InstallableManager;
  // clients must copy the bitmap if they want to to use it.
  raw_ptr<const SkBitmap> primary_icon;

  // Whether the primary icon had the 'maskable' purpose, meaningless if no
  // primary_icon was requested.
  const bool has_maskable_primary_icon;

  // The URL of the chosen splash icon.
  const GURL& splash_icon_url;

  // nullptr if the most appropriate splash icon couldn't be determined or
  // downloaded. The underlying splash icon is owned by the InstallableManager;
  // clients must copy the bitmap if they want to use it. Since the splash
  // icon is optional, no error code is set if it cannot be fetched, and clients
  // specifying |valid_splash_icon| must check that the bitmap exists before
  // using it.
  raw_ptr<const SkBitmap> splash_icon;

  // Whether the splash icon had the 'maskable' purpose, meaningless if no
  // splash_icon was requested.
  const bool has_maskable_splash_icon;

  // The screenshots to show in the install UI.
  const std::vector<SkBitmap>& screenshots;

  // true if the site has a valid, installable web app manifest. If
  // |valid_manifest| or |worker_check_passed| was true and the site isn't
  // installable, the reason will be in |errors|.
  const bool valid_manifest = false;

  // true if the site has a service worker with a fetch handler or
  // the service worker check was not requested when fetching this data.
  const bool worker_check_passed = false;
};

using InstallableCallback = base::OnceCallback<void(const InstallableData&)>;

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_DATA_H_
