// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEB_APP_INTERNALS_WEB_APP_INTERNALS_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_WEB_APP_INTERNALS_WEB_APP_INTERNALS_SOURCE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/url_data_source.h"

class Profile;

// A simple JSON data source that returns web app debugging information for the
// associated profile.
class WebAppInternalsSource : public content::URLDataSource {
 public:
  static void BuildWebAppInternalsJson(
      Profile* profile,
      base::OnceCallback<void(base::Value root)> callback);

  explicit WebAppInternalsSource(Profile* profile);
  WebAppInternalsSource(const WebAppInternalsSource&) = delete;
  WebAppInternalsSource& operator=(const WebAppInternalsSource&) = delete;
  ~WebAppInternalsSource() override;

  // content::URLDataSource:
  std::string GetSource() override;
  std::string GetMimeType(const GURL& url) override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;

 private:
  const raw_ptr<Profile> profile_;

  base::WeakPtrFactory<WebAppInternalsSource> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEB_APP_INTERNALS_WEB_APP_INTERNALS_SOURCE_H_
