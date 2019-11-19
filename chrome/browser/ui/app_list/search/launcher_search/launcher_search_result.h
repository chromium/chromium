// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_LAUNCHER_SEARCH_LAUNCHER_SEARCH_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_LAUNCHER_SEARCH_LAUNCHER_SEARCH_RESULT_H_

#include <memory>
#include <string>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/launcher_search/launcher_search_icon_image_loader.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

namespace app_list {

class LauncherSearchResult : public ChromeSearchResult,
                             public LauncherSearchIconImageLoader::Observer {
 public:
  LauncherSearchResult(
      const std::string& item_id,
      const GURL& icon_url,
      const int discrete_value_relevance,
      Profile* profile,
      const extensions::Extension* extension,
      std::unique_ptr<chromeos::launcher_search_provider::ErrorReporter>
          error_reporter);
  ~LauncherSearchResult() override;
  std::unique_ptr<LauncherSearchResult> Duplicate() const;

  // ChromeSearchResult overrides:
  void Open(int event_flags) override;
  ash::SearchResultType GetSearchResultType() const override;

  void OnIconImageChanged(LauncherSearchIconImageLoader* image_loader) override;
  void OnBadgeIconImageChanged(
      LauncherSearchIconImageLoader* image_loader) override;

 private:
  // Constructor for duplicating a result.
  LauncherSearchResult(
      const std::string& item_id,
      const int discrete_value_relevance,
      Profile* profile,
      const extensions::Extension* extension,
      const scoped_refptr<LauncherSearchIconImageLoader>& icon_image_loader);
  void Initialize();

  // Returns search result ID. The search result ID is comprised of the
  // extension ID and the extension-supplied item ID. This is to avoid naming
  // collisions for results of different extensions.
  std::string GetSearchResultId();

  const std::string item_id_;
  // Must be between 0 and kMaxSearchResultScore.
  const int discrete_value_relevance_;
  Profile* profile_;
  const extensions::Extension* extension_;
  scoped_refptr<LauncherSearchIconImageLoader> icon_image_loader_;

  DISALLOW_COPY_AND_ASSIGN(LauncherSearchResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_LAUNCHER_SEARCH_LAUNCHER_SEARCH_RESULT_H_
