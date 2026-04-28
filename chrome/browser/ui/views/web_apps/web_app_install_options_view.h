// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_OPTIONS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_OPTIONS_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_flow_dialog_delegate.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

namespace views {
class Checkbox;
}

namespace web_app {

// A view that presents installation options to the user.
// The content varies based on the platform (InstallOsType).
class WebAppInstallOptionsView : public views::View {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kViewId);

  WebAppInstallOptionsView(InstallOsType os_type,
                           const std::u16string& title,
                           const gfx::ImageSkia& icon_image);
  ~WebAppInstallOptionsView() override;

  WebAppInstallOptionsView(const WebAppInstallOptionsView&) = delete;
  WebAppInstallOptionsView& operator=(const WebAppInstallOptionsView&) = delete;

  // Returns true if the "Pin to shelf" option is checked.
  // Only applicable for kCrOS.
  bool IsPinToShelfChecked() const;

  void SetPinToShelfCheckedForTesting(bool checked);

  base::WeakPtr<WebAppInstallOptionsView> GetWeakPtr();

  // Returns true if the "Add desktop shortcut" option is checked.
  // Only applicable for kWin.
  bool IsAddDesktopShortcutChecked() const;

  // Returns true if the "Pin to Task Bar" option is checked.
  // Only applicable for kWin.
  bool IsPinToTaskBarChecked() const;

 private:
  void InitView(InstallOsType os_type,
                const std::u16string& title,
                const gfx::ImageSkia& icon_image);

  raw_ptr<views::Checkbox> pin_to_shelf_checkbox_ = nullptr;
  raw_ptr<views::Checkbox> add_desktop_shortcut_checkbox_ = nullptr;
  raw_ptr<views::Checkbox> pin_to_task_bar_checkbox_ = nullptr;

  base::WeakPtrFactory<WebAppInstallOptionsView> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_OPTIONS_VIEW_H_
