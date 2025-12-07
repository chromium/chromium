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
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/browser/ui/views/web_apps/protocol_handler_picker_dialog.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/image_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "url/gurl.h"

namespace {

constexpr int kIconSizeInDip = 32;

void MaybeCloseTabAsync(tabs::TabInterface& tab) {
  // If there's more than one tab in the browser corresponding to the current
  // tab and the current tab still in the initial navigation state, it's
  // expected to be closed.
  if (tab.GetContents()->GetController().IsInitialNavigation()) {
    base::UmaHistogramBoolean("WebApp.ProtocolHandlerPicker.TabClosed", true);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&tabs::TabInterface::Close, tab.GetWeakPtr()));
    return;
  }
  base::UmaHistogramBoolean("WebApp.ProtocolHandlerPicker.TabClosed", false);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ProtocolHandlerPickerDialogClosedReason)
enum class ProtocolHandlerPickerDialogClosedReason {
  kOkButtonClicked = 0,
  kCancelButtonClicked = 1,
  kOther = 2,
  kMaxValue = kOther
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webapps/enums.xml:ProtocolHandlerPickerDialogClosedReason)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ProtocolHandlerPickerAction)
enum class ProtocolHandlerPickerAction {
  kPreferredAppLaunched = 0,
  kDialogShown = 1,
  kAbortedDialogAlreadyOpen = 2,
  kMaxValue = kAbortedDialogAlreadyOpen
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webapps/enums.xml:ProtocolHandlerPickerAction)

ProtocolHandlerPickerDialogClosedReason CloseReasonAsHistogramEnum(
    views::Widget::ClosedReason reason) {
  switch (reason) {
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      return ProtocolHandlerPickerDialogClosedReason::kOkButtonClicked;
    case views::Widget::ClosedReason::kCancelButtonClicked:
      return ProtocolHandlerPickerDialogClosedReason::kCancelButtonClicked;
    default:
      return ProtocolHandlerPickerDialogClosedReason::kOther;
  }
}

void RecordPickerAction(ProtocolHandlerPickerAction action) {
  base::UmaHistogramEnumeration("WebApp.ProtocolHandlerPicker.Action", action);
}

std::optional<std::u16string> SerializeInitiatorOriginWithUrlIdentity(
    Profile* profile,
    const std::optional<url::Origin>& initiator_origin) {
  if (!initiator_origin) {
    return std::nullopt;
  }
  return UrlIdentity::CreateFromUrl(
             profile, initiator_origin->GetURL(),
             {UrlIdentity::Type::kDefault, UrlIdentity::Type::kIsolatedWebApp,
              UrlIdentity::Type::kChromeExtension},
             {.default_options = {}})
      .name;
}

}  // namespace

namespace web_app {

DEFINE_USER_DATA(ProtocolHandlerPickerCoordinator);

void ProtocolHandlerPickerCoordinator::ShowPicker(
    const GURL& protocol_url,
    const std::vector<std::string>& app_ids,
    const std::optional<url::Origin>& initiator_origin) {
  GatherAppData(protocol_url, app_ids, initiator_origin);
}

std::optional<std::string> ProtocolHandlerPickerCoordinator::FindPreferredApp(
    const GURL& protocol_url,
    const std::vector<std::string>& app_ids) {
  if (std::optional<std::string> app_id =
          proxy_->PreferredAppsList().FindPreferredAppForUrl(protocol_url)) {
    if (base::Contains(app_ids, *app_id)) {
      return app_id;
    }
  }
  return std::nullopt;
}

void ProtocolHandlerPickerCoordinator::LaunchApp(
    const GURL& protocol_url,
    const std::string& app_id,
    std::optional<ConfirmationDialogAction> action) {
  apps::AppLaunchParams params(app_id,
                               apps::LaunchContainer::kLaunchContainerWindow,
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               apps::LaunchSource::kFromProtocolHandler);
  params.protocol_handler_launch_url = protocol_url;
  params.confirmation_dialog_action = action;
  proxy_->LaunchAppWithParams(std::move(params));
}

ProtocolHandlerPickerCoordinator::ProtocolHandlerPickerCoordinator(
    tabs::TabInterface& tab,
    apps::AppServiceProxy* proxy)
    : tab_(tab),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this),
      proxy_(*proxy) {}

ProtocolHandlerPickerCoordinator::~ProtocolHandlerPickerCoordinator() = default;

// static
ProtocolHandlerPickerCoordinator* ProtocolHandlerPickerCoordinator::From(
    tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

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
  if (HasOpenDialogWidget()) {
    RecordPickerAction(ProtocolHandlerPickerAction::kAbortedDialogAlreadyOpen);
    return;
  }

  std::unique_ptr<ui::DialogModel> dialog_model =
      CreateProtocolHandlerPickerDialog(
          protocol_url, app_entries,
          SerializeInitiatorOriginWithUrlIdentity(
              Profile::FromBrowserContext(
                  tab_->GetContents()->GetBrowserContext()),
              initiator_origin),
          base::BindOnce(
              &ProtocolHandlerPickerCoordinator::OnPreferredHandlerSelected,
              weak_factory_.GetWeakPtr(), protocol_url));

  views::BubbleDialogModelHost* model_host =
      views::BubbleDialogModelHost::CreateModal(std::move(dialog_model),
                                                ui::mojom::ModalType::kChild)
          .release();
  model_host->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  auto params = std::make_unique<tabs::TabDialogManager::Params>();
  params->close_on_detach = false;

  dialog_ = tab_->GetTabFeatures()->tab_dialog_manager()->CreateAndShowDialog(
      model_host, std::move(params));

  RecordPickerAction(ProtocolHandlerPickerAction::kDialogShown);

  // This will be called after the callback supplied to
  // `CreateProtocolHandlerPickerDialog()`.
  dialog_->MakeCloseSynchronous(
      base::BindOnce(&ProtocolHandlerPickerCoordinator::CloseDialogWidget,
                     weak_factory_.GetWeakPtr()));
  // By default, the dialog may not have its initially focused view
  // actually focused on some platforms (see crbug.com/440104083).
  // Explicitly call RequestFocus() to ensure this happens.
  model_host->GetInitiallyFocusedView()->RequestFocus();
}

bool ProtocolHandlerPickerCoordinator::HasOpenDialogWidget() const {
  return dialog_ && !dialog_->IsClosed();
}

void ProtocolHandlerPickerCoordinator::OnPreferredHandlerSelected(
    const GURL& protocol_url,
    const std::string& app_id,
    bool remember_choice) {
  base::UmaHistogramBoolean("WebApp.ProtocolHandlerPicker.RememberSelection",
                            remember_choice);
  auto action = ConfirmationDialogAction::kForceSkip;
  if (remember_choice) {
    proxy_->SetProtocolLinkPreference(app_id, protocol_url.scheme());
    action = ConfirmationDialogAction::kPersistentlyForceSkip;
  }

  LaunchApp(protocol_url, app_id, action);
}

void ProtocolHandlerPickerCoordinator::CloseDialogWidget(
    views::Widget::ClosedReason reason) {
  base::UmaHistogramEnumeration(
      "WebApp.ProtocolHandlerPicker.DialogClosedReason",
      CloseReasonAsHistogramEnum(reason));

  dialog_.reset();

  // If this is a new tab just for protocol navigations, it has to be closed
  // now; this might lead to this class getting destroyed too.
  MaybeCloseTabAsync(*tab_);
}

void LaunchProtocolUrlInPreferredApp(
    content::WebContents* web_contents,
    const GURL& protocol_url,
    const std::vector<std::string>& app_ids,
    const std::optional<url::Origin>& initiator_origin) {
  auto* tab = tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab) {
    return;
  }

  auto* picker_coordinator = ProtocolHandlerPickerCoordinator::From(tab);
  CHECK(picker_coordinator) << " The caller is responsible for making sure "
                               "that the coordinator exists.";

  if (std::optional<std::string> app_id =
          picker_coordinator->FindPreferredApp(protocol_url, app_ids)) {
    RecordPickerAction(ProtocolHandlerPickerAction::kPreferredAppLaunched);
    picker_coordinator->LaunchApp(protocol_url, *app_id);
    MaybeCloseTabAsync(*tab);
    return;
  }

  picker_coordinator->ShowPicker(protocol_url, app_ids, initiator_origin);
}

}  // namespace web_app
