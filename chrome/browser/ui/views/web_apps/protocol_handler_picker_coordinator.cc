// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/protocol_handler_picker_coordinator.h"

#include <string>
#include <vector>

#include "base/barrier_callback.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/web_apps/protocol_handler_picker_dialog.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/image_model.h"
#include "url/gurl.h"

namespace {

constexpr int kIconSizeInDip = 32;

void LaunchApp(apps::AppServiceProxy* proxy,
               const std::string& app_id,
               const GURL& protocol_url) {
  apps::AppLaunchParams params(app_id,
                               apps::LaunchContainer::kLaunchContainerWindow,
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               apps::LaunchSource::kFromProtocolHandler);
  params.protocol_handler_launch_url = protocol_url;
  proxy->LaunchAppWithParams(std::move(params));
}

void MaybeCloseWebContentsAsync(content::WebContents* web_contents) {
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab) {
    return;
  }
  TabStripModel* tab_strip_model =
      tab->GetBrowserWindowInterface()->GetTabStripModel();
  // If there's more than one tab in the browser corresponding to the current
  // tab and the current tab still in the initial navigation state, it's
  // expected to be closed.
  if (tab_strip_model->GetIndexOfTab(tab) != TabStripModel::kNoTab &&
      tab_strip_model->count() > 1 &&
      web_contents->GetController().IsInitialNavigation()) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&content::WebContents::Close,
                                  web_contents->GetWeakPtr()));
  }
}

}  // namespace

namespace web_app {

void ProtocolHandlerPickerCoordinator::ShowPicker(
    const GURL& protocol_url,
    const std::vector<std::string>& app_ids,
    const std::optional<url::Origin>& initiator_origin) {
  GatherAppData(protocol_url, app_ids, initiator_origin);
}

ProtocolHandlerPickerCoordinator::ProtocolHandlerPickerCoordinator(
    content::WebContents* web_contents)
    : content::WebContentsUserData<ProtocolHandlerPickerCoordinator>(
          *web_contents),
      proxy_(apps::AppServiceProxyFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
  CHECK(proxy_) << " AppServiceProxy must exist for this instance of "
                   "WebContents; the caller is responsible for ensuring this.";
}

ProtocolHandlerPickerCoordinator::~ProtocolHandlerPickerCoordinator() = default;

void ProtocolHandlerPickerCoordinator::GatherAppData(
    const GURL& protocol_url,
    const std::vector<std::string>& app_ids,
    const std::optional<url::Origin>& initiator_origin) {
  CHECK(!app_ids.empty());
  auto all_apps_collected_callback =
      base::BarrierCallback<ProtocolHandlerPickerDialogEntry>(
          app_ids.size(),
          base::BindOnce(
              &ProtocolHandlerPickerCoordinator::ShowPickerWithEntries,
              weak_factory_.GetWeakPtr(), protocol_url, initiator_origin));

  for (const std::string& app_id : app_ids) {
    std::u16string app_name;
    proxy_->AppRegistryCache().ForOneApp(
        app_id, [&app_name](const apps::AppUpdate& update) {
          app_name = base::UTF8ToUTF16(update.Name());
        });

    proxy_->LoadIcon(
        app_id, apps::IconType::kStandard, kIconSizeInDip,
        /*allow_placeholder_icon=*/false,
        base::BindOnce(
            [](const std::string& app_id, const std::u16string& app_name,
               apps::IconValuePtr icon_value)
                -> ProtocolHandlerPickerDialogEntry {
              return {app_id, app_name,
                      ui::ImageModel::FromImageSkia(icon_value->uncompressed)};
            },
            app_id, app_name)
            .Then(all_apps_collected_callback));
  }
}

void ProtocolHandlerPickerCoordinator::ShowPickerWithEntries(
    const GURL& protocol_url,
    const std::optional<url::Origin>& initiator_origin,
    ProtocolHandlerPickerDialogEntries app_entries) {
  auto dialog_model = CreateProtocolHandlerPickerDialog(
      protocol_url, app_entries, initiator_origin,
      base::BindOnce(&ProtocolHandlerPickerCoordinator::OnPickerClosed,
                     weak_factory_.GetWeakPtr(), protocol_url));
  // TODO(cbug.com/422422887): Attach `dialog_model` to `tab_dialog_manager()`.
}

void ProtocolHandlerPickerCoordinator::OnPickerClosed(
    const GURL& protocol_url,
    std::optional<ProtocolHandlerPickerDialogResult> result) {
  if (result) {
    const auto& app_id = result->selected_app_id;
    if (result->remember_choice) {
      // TODO(cbug.com/422422887): Store the user pref in PreferredAppsList.
    }
    LaunchApp(proxy_, app_id, protocol_url);
  }

  MaybeCloseWebContentsAsync(&GetWebContents());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ProtocolHandlerPickerCoordinator);

void LaunchProtocolUrlInPreferredApp(
    content::WebContents* web_contents,
    const GURL& protocol_url,
    const std::vector<std::string>& app_ids,
    const std::optional<url::Origin>& initiator_origin) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (std::optional<std::string> app_id =
          proxy->PreferredAppsList().FindPreferredAppForUrl(protocol_url)) {
    if (base::Contains(app_ids, *app_id)) {
      LaunchApp(proxy, *app_id, protocol_url);
      MaybeCloseWebContentsAsync(web_contents);
      return;
    }
  }

  ProtocolHandlerPickerCoordinator::GetOrCreateForWebContents(web_contents)
      ->ShowPicker(protocol_url, app_ids, initiator_origin);
}

}  // namespace web_app
