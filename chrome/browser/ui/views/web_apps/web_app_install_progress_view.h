// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_PROGRESS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_PROGRESS_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/callback_delayer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout_view.h"

namespace web_app {

class WebAppInstallProgressView : public views::View {
  METADATA_HEADER(WebAppInstallProgressView, views::View)

 public:
  WebAppInstallProgressView();
  ~WebAppInstallProgressView() override;

  void SetProgressValue(double progress);

  base::WeakPtr<WebAppInstallProgressView> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  raw_ptr<views::ProgressBar> progress_bar_ = nullptr;
  base::WeakPtrFactory<WebAppInstallProgressView> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_PROGRESS_VIEW_H_
