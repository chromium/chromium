// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_INTRO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_INTRO_VIEW_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace views {
class Textfield;
}  // namespace views

class GURL;

namespace web_app {

class WebAppScreenshotFetcher;

// A view that shows the app icon, name, origin, and a description for the
// install dialog.
class WebAppInstallIntroView : public views::View {
  METADATA_HEADER(WebAppInstallIntroView, views::View)
 public:
  static std::unique_ptr<WebAppInstallIntroView> Create(
      InstallDialogType install_type,
      const gfx::ImageSkia& icon_image,
      const std::u16string& app_name,
      const GURL& start_url,
      bool is_maskable,
      const std::u16string& description,
      base::WeakPtr<WebAppScreenshotFetcher> fetcher,
      base::RepeatingCallback<void(const std::u16string&)>
          text_tracker_callback);
  ~WebAppInstallIntroView() override;

  views::Textfield* textfield() const { return textfield_; }

 private:
  WebAppInstallIntroView(InstallDialogType install_type,
                         const gfx::ImageSkia& icon_image,
                         const std::u16string& app_name,
                         const GURL& start_url,
                         bool is_maskable,
                         const std::u16string& description,
                         base::WeakPtr<WebAppScreenshotFetcher> fetcher,
                         base::RepeatingCallback<void(const std::u16string&)>
                             text_tracker_callback);

  raw_ptr<views::Textfield> textfield_ = nullptr;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_INTRO_VIEW_H_
