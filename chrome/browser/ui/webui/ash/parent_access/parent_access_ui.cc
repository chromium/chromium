// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/webui/common/trusted_types_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_dialog.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.mojom.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui_handler_impl.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/parent_access_resources.h"
#include "chrome/grit/parent_access_resources_map.h"
#include "chrome/grit/supervision_resources.h"
#include "chrome/grit/supervision_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace ash {

// static
signin::IdentityManager* ParentAccessUI::test_identity_manager_ = nullptr;

ParentAccessUI::ParentAccessUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  // Set up the basic page framework.
  SetUpResources();
}

ParentAccessUI::~ParentAccessUI() = default;

// static
void ParentAccessUI::SetUpForTest(signin::IdentityManager* identity_manager) {
  test_identity_manager_ = identity_manager;
}

void ParentAccessUI::BindInterface(
    mojo::PendingReceiver<parent_access_ui::mojom::ParentAccessUiHandler>
        receiver) {
  signin::IdentityManager* identity_manager =
      test_identity_manager_
          ? test_identity_manager_
          : IdentityManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()));

  // The dialog instance could be null if the webui's url is entered in the
  // browser address bar.  The handler should handle that scenario.
  mojo_api_handler_ = std::make_unique<ParentAccessUiHandlerImpl>(
      std::move(receiver), identity_manager, ParentAccessDialog::GetInstance());
}

void ParentAccessUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

parent_access_ui::mojom::ParentAccessUiHandler*
ParentAccessUI::GetHandlerForTest() {
  return mojo_api_handler_.get();
}

void ParentAccessUI::SetUpResources() {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui()), chrome::kChromeUIParentAccessHost);
  ash::EnableTrustedTypesCSP(source);

  source->EnableReplaceI18nInJS();

  // Forward data to the WebUI.
  source->AddResourcePaths(
      base::make_span(kParentAccessResources, kParentAccessResourcesSize));
  source->AddResourcePaths(
      base::make_span(kSupervisionResources, kSupervisionResourcesSize));

  source->UseStringsJs();
  source->AddBoolean("isParentAccessJellyEnabled",
                     features::IsParentAccessJellyEnabled());
  source->SetDefaultResource(IDR_PARENT_ACCESS_PARENT_ACCESS_HTML);

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"pageTitle", IDS_PARENT_ACCESS_PAGE_TITLE},
      {"closeButtonTitle", IDS_PARENT_ACCESS_CLOSE_BUTTON_TITLE},
      {"approveButtonText", IDS_PARENT_ACCESS_AFTER_APPROVE_BUTTON},
      {"denyButtonText", IDS_PARENT_ACCESS_AFTER_DENY_BUTTON},
      {"askInPersonButtonText", IDS_PARENT_ACCESS_ASK_IN_PERSON_BUTTON},
      {"okButtonText", IDS_PARENT_ACCESS_OK_BUTTON},
      {"localWebApprovalsAfterTitle",
       IDS_PARENT_ACCESS_LOCAL_WEB_APPROVALS_AFTER_TITLE},
      {"localWebApprovalsAfterSubtitle",
       IDS_PARENT_ACCESS_LOCAL_WEB_APPROVALS_AFTER_SUBTITLE},
      {"localWebApprovalsAfterDetails",
       IDS_PARENT_ACCESS_LOCAL_WEB_APPROVALS_AFTER_DETAILS},
      {"webviewLoadingMessage", IDS_PARENT_ACCESS_WEBVIEW_LOADING_MESSAGE},
      {"supervisedUserOfflineTitle", IDS_SUPERVISED_USER_OFFLINE_TITLE},
      {"supervisedUserOfflineDescription",
       IDS_SUPERVISED_USER_OFFLINE_DESCRIPTION},
      {"supervisedUserErrorTitle", IDS_SUPERVISED_USER_ERROR_TITLE},
      {"supervisedUserErrorDescription", IDS_SUPERVISED_USER_ERROR_DESCRIPTION},
      {"extensionApprovalsDisabledTitle",
       IDS_PARENT_ACCESS_EXTENSION_APPROVALS_DISABLED_TITLE},
      {"extensionApprovalsDisabledSubtitle",
       IDS_PARENT_ACCESS_EXTENSION_APPROVALS_DISABLED_SUBTITLE},
      {"extensionApprovalsBeforeTitle",
       IDS_PARENT_ACCESS_EXTENSION_APPROVALS_BEFORE_TITLE},
      {"extensionApprovalsBeforeSubtitle",
       IDS_PARENT_ACCESS_EXTENSION_APPROVALS_BEFORE_SUBTITLE},
      {"extensionApprovalsAfterTitle",
       IDS_PARENT_ACCESS_EXTENSION_APPROVALS_AFTER_TITLE},
      {"extensionApprovalsPermissionsHeader",
       IDS_PARENT_ACCESS_EXTENSION_PERMISSIONS_HEADER},
      {"extensionApprovalsShowDetailsButton",
       IDS_PARENT_ACCESS_EXTENSION_PERMISSION_SHOW_DETAILS},
      {"extensionApprovalsHideDetailsButton",
       IDS_PARENT_ACCESS_EXTENSION_PERMISSION_HIDE_DETAILS}};

  source->AddLocalizedStrings(kLocalizedStrings);

  // Enables use of test_loader.html
  webui::SetJSModuleDefaults(source);

  // Allows loading of local content into an iframe for testing.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc,
      "frame-src chrome://webui-test/;");
}

WEB_UI_CONTROLLER_TYPE_IMPL(ParentAccessUI)

}  // namespace ash
