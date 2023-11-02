// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PREFS_INTERNALS_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_PREFS_INTERNALS_SOURCE_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/url_data_source.h"

class Profile;

// A simple data source that returns the preferences for the associated profile.
class PrefsInternalsSource : public content::URLDataSource {
 public:
  explicit PrefsInternalsSource(Profile* profile);

  PrefsInternalsSource(const PrefsInternalsSource&) = delete;
  PrefsInternalsSource& operator=(const PrefsInternalsSource&) = delete;

  ~PrefsInternalsSource() override;

  // content::URLDataSource:
  std::string GetSource() override;
  std::string GetMimeType(const GURL& url) override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;

 private:
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PREFS_INTERNALS_SOURCE_H_
