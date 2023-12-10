// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_SUB_APPS_INSTALL_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_SUB_APPS_INSTALL_DIALOG_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}

class Profile;

namespace web_app {

struct WebAppInstallInfo;

class SubAppsInstallDialogController : public views::WidgetObserver {
 public:
  enum class DialogActionForTesting { kAccept, kCancel };
  enum class SubAppsInstallDialogViewID : int {
    VIEW_ID_NONE = 0,
    SUB_APP_LABEL,
    SUB_APP_ICON,
    MANAGE_PERMISSIONS_LINK,
  };

  static base::AutoReset<std::optional<DialogActionForTesting>>
  SetAutomaticActionForTesting(DialogActionForTesting auto_accept);

  SubAppsInstallDialogController();
  SubAppsInstallDialogController(const SubAppsInstallDialogController&) =
      delete;
  SubAppsInstallDialogController& operator=(
      const SubAppsInstallDialogController&) = delete;
  ~SubAppsInstallDialogController() override;

  void Init(base::OnceCallback<void(bool)> callback,
            const std::vector<std::unique_ptr<WebAppInstallInfo>>& sub_apps,
            const std::string& parent_app_name,
            const webapps::AppId& parent_app_id,
            Profile* profile,
            gfx::NativeWindow window);

  views::Widget* GetWidgetForTesting();

 private:
  // `views::WidgetObserver`:
  void OnWidgetDestroying(views::Widget* widget) override;

  base::OnceCallback<void(bool)> callback_;

  raw_ptr<views::Widget> widget_ = nullptr;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_SUB_APPS_INSTALL_DIALOG_CONTROLLER_H_
