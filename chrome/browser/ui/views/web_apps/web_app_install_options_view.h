// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_OPTIONS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_OPTIONS_VIEW_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_flow_dialog_delegate.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

namespace views {
class Checkbox;
class ImageView;
}  // namespace views

namespace gfx {
class ImageSkia;
}

class GURL;

namespace web_app {

// A view that presents installation options to the user.
// The content varies based on the platform (InstallOsType).
class WebAppInstallOptionsView : public views::View {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kViewId);
  static std::unique_ptr<WebAppInstallOptionsView> Create(
      InstallOsType os_type,
      const std::u16string& title,
      const gfx::ImageSkia& icon_image,
      const gfx::ImageSkia& large_icon_image,
      bool is_maskable,
      const GURL& start_url);
  ~WebAppInstallOptionsView() override;

  WebAppInstallOptionsView(const WebAppInstallOptionsView&) = delete;
  WebAppInstallOptionsView& operator=(const WebAppInstallOptionsView&) = delete;

  // Returns true if the "Pin to shelf" option is checked.
  // Only applicable for kCrOS.
  bool IsPinToShelfChecked() const;

  void SetPinToShelfCheckedForTesting(bool checked);

  // Returns true if the "Add desktop shortcut" option is checked.
  // Only applicable for kWin.
  bool IsAddDesktopShortcutChecked() const;

  // Returns true if the "Pin to Task Bar" option is checked.
  // Only applicable for kWin.
  bool IsPinToTaskBarChecked() const;

  void SetAddDesktopShortcutCheckedForTesting(bool checked);
  void SetPinToTaskBarCheckedForTesting(bool checked);

  base::WeakPtr<WebAppInstallOptionsView> GetWeakPtr();

 private:
  WebAppInstallOptionsView(InstallOsType os_type,
                           const std::u16string& title,
                           const gfx::ImageSkia& icon_image,
                           const gfx::ImageSkia& large_icon_image,
                           bool is_maskable,
                           const GURL& start_url);
  void MaybeApplyOsIconMasking(const gfx::ImageSkia& icon_image,
                               bool is_maskable);
  void OnIconMaskingCompleteWithShadow(SkBitmap masked_bitmap);

  raw_ptr<views::Checkbox> pin_to_shelf_checkbox_ = nullptr;
  raw_ptr<views::Checkbox> add_desktop_shortcut_checkbox_ = nullptr;
  raw_ptr<views::Checkbox> pin_to_task_bar_checkbox_ = nullptr;
  raw_ptr<views::ImageView> icon_view_ = nullptr;

  base::WeakPtrFactory<WebAppInstallOptionsView> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_OPTIONS_VIEW_H_
