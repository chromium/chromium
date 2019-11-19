// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SCREENLOCK_ICON_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SCREENLOCK_ICON_SOURCE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/url_data_source.h"

namespace chromeos {

class ScreenlockIconProvider;

// A URL data source that serves icon images for the screenlockPrivate API.
class ScreenlockIconSource : public content::URLDataSource {
 public:
  explicit ScreenlockIconSource(
      base::WeakPtr<ScreenlockIconProvider> icon_provider);
  ~ScreenlockIconSource() override;

  // content::URLDataSource implementation.
  std::string GetSource() const override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      const content::URLDataSource::GotDataCallback& callback) override;

  std::string GetMimeType(const std::string& path) const override;

  // Constructs and returns the icon URL for a given user.
  static std::string GetIconURLForUser(const std::string& username);

 private:
  base::WeakPtr<ScreenlockIconProvider> icon_provider_;

  DISALLOW_COPY_AND_ASSIGN(ScreenlockIconSource);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SCREENLOCK_ICON_SOURCE_H_
