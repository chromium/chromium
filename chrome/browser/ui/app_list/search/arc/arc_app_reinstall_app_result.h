// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_APP_REINSTALL_APP_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_APP_REINSTALL_APP_RESULT_H_

#include <string>

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "ui/gfx/image/image_skia.h"

namespace app_list {

// A ChromeSearchResult that shows an App Reinstall candidate result. These are
// Arc++ apps that can be installed on this device. Opens the app in the play
// store when Open is called.
class ArcAppReinstallAppResult : public ChromeSearchResult {
 public:
  class Observer {
   public:
    virtual void OnOpened(const std::string& package_name) = 0;
    virtual void OnVisibilityChanged(const std::string& package_name,
                                     bool visibility) = 0;

   protected:
    virtual ~Observer() = default;
  };

  ArcAppReinstallAppResult(
      const arc::mojom::AppReinstallCandidatePtr& mojom_data,
      const gfx::ImageSkia& skia_icon,
      ArcAppReinstallAppResult::Observer* observer);

  ArcAppReinstallAppResult(const ArcAppReinstallAppResult&) = delete;
  ArcAppReinstallAppResult& operator=(const ArcAppReinstallAppResult&) = delete;

  ~ArcAppReinstallAppResult() override;

  // ChromeSearchResult:
  void Open(int event_flags) override;
  void OnVisibilityChanged(bool visibility) override;

 private:
  // Observer passed in constructor. not owned.
  Observer* const observer_;
  const std::string package_name_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_APP_REINSTALL_APP_RESULT_H_
