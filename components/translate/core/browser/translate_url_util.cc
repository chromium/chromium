// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_url_util.h"

#include "components/translate/core/browser/translate_download_manager.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"

namespace translate {

namespace {

// Used in all translate URLs to specify API Key.
const char kApiKeyName[] = "key";

// Used in kTranslateScriptURL and kLanguageListFetchURL to specify the
// application locale.
const char kHostLocaleQueryName[] = "hl";

}  // namespace

GURL AddApiKeyToUrl(const GURL& url) {
  return net::AppendQueryParameter(url, kApiKeyName, google_apis::GetAPIKey());
}

GURL AddHostLocaleToUrl(const GURL& url) {
  return net::AppendQueryParameter(
      url,
      kHostLocaleQueryName,
      TranslateDownloadManager::GetLanguageCode(
          TranslateDownloadManager::GetInstance()->application_locale()));
}

}  // namespace translate
