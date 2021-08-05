// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PREFS_INTERNALS_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_PREFS_INTERNALS_SOURCE_H_

#include "base/macros.h"
#include "content/public/browser/url_data_source.h"

class Profile;

// A simple data source that returns the preferences for the associated profile.
class PrefsInternalsSource : public content::URLDataSource {
 public:
  explicit PrefsInternalsSource(Profile* profile);
  ~PrefsInternalsSource() override;

  // content::URLDataSource:
  std::string GetSource() override;
  std::string GetMimeType(const std::string& path) override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(PrefsInternalsSource);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PREFS_INTERNALS_SOURCE_H_
