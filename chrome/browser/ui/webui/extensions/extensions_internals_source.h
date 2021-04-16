// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSIONS_INTERNALS_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSIONS_INTERNALS_SOURCE_H_

#include <string>

#include "content/public/browser/url_data_source.h"

class Profile;

// A simple data source that returns information about installed
// extensions for the associated profile.
class ExtensionsInternalsSource : public content::URLDataSource {
 public:
  explicit ExtensionsInternalsSource(Profile* profile);
  ExtensionsInternalsSource(const ExtensionsInternalsSource&) = delete;
  ExtensionsInternalsSource& operator=(const ExtensionsInternalsSource&) =
      delete;
  ~ExtensionsInternalsSource() override;

  // content::URLDataSource:
  std::string GetSource() override;
  std::string GetMimeType(const std::string& path) override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;

  // Simpler interface to generate string output, without needing to
  // call StartDataRequest.
  std::string WriteToString() const;

 private:
  Profile* const profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSIONS_INTERNALS_SOURCE_H_
