// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_PROTOCOL_HANDLER_PICKER_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_PROTOCOL_HANDLER_PICKER_COORDINATOR_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/ui/views/web_apps/protocol_handler_picker_dialog.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

// This class manages the UI flow for picking a web app to handle a protocol
// request. Its lifetime is tied to the TabInterface it's attached to.
class ProtocolHandlerPickerCoordinator {
 public:
  using ConfirmationDialogAction =
      apps::AppLaunchParams::ConfirmationDialogAction;

  ProtocolHandlerPickerCoordinator(tabs::TabInterface& tab,
                                   apps::AppServiceProxy* proxy);
  ~ProtocolHandlerPickerCoordinator();

  DECLARE_USER_DATA(ProtocolHandlerPickerCoordinator);
  static ProtocolHandlerPickerCoordinator* From(tabs::TabInterface* tab);

  void ShowPicker(const GURL& protocol_url,
                  const std::vector<std::string>& app_ids,
                  const std::optional<url::Origin>& initiating_origin);

  std::optional<std::string> FindPreferredApp(
      const GURL& protocol_url,
      const std::vector<std::string>& app_ids);

  void LaunchApp(const GURL& protocol_url,
                 const std::string& app_id,
                 std::optional<ConfirmationDialogAction> = std::nullopt);

 private:
  void GatherAppData(const GURL& protocol_url,
                     const std::vector<std::string>& app_ids,
                     const std::optional<url::Origin>& initiating_origin);
  void ShowPickerWithEntries(
      const GURL& protocol_url,
      const std::optional<url::Origin>& initiating_origin,
      ProtocolHandlerPickerDialogEntries app_entries);
  void OnPreferredHandlerSelected(const GURL& protocol_url,
                                  const std::string& app_id,
                                  bool remember_selection);
  bool HasOpenDialogWidget() const;
  void CloseDialogWidget(views::Widget::ClosedReason reason);

  const raw_ref<tabs::TabInterface> tab_;
  ui::ScopedUnownedUserData<ProtocolHandlerPickerCoordinator>
      scoped_unowned_user_data_;

  const raw_ref<apps::AppServiceProxy> proxy_;

  // Owns the currently opened dialog for this tab.
  std::unique_ptr<views::Widget> dialog_;

  base::WeakPtrFactory<ProtocolHandlerPickerCoordinator> weak_factory_{this};
};

// If there's a preferred app among `app_ids` for handling `protocol_url`,
// launches this app directly; otherwise shows the protocol handler picker
// dialog.
void LaunchProtocolUrlInPreferredApp(
    content::WebContents* web_contents,
    const GURL& protocol_url,
    const std::vector<std::string>& app_ids,
    const std::optional<url::Origin>& initiator_origin);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_PROTOCOL_HANDLER_PICKER_COORDINATOR_H_
