// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/data_sharing/data_sharing_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/data_sharing/data_sharing_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/data_sharing_resources.h"
#include "chrome/grit/data_sharing_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/data_sharing/public/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/webui_allowlist.h"
#include "ui/webui/webui_util.h"

DataSharingUIConfig::DataSharingUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIUntrustedScheme,
                                  chrome::kChromeUIUntrustedDataSharingHost) {}

DataSharingUIConfig::~DataSharingUIConfig() = default;

bool DataSharingUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(
      data_sharing::features::kDataSharingFeature);
}

bool DataSharingUIConfig::ShouldAutoResizeHost() {
  return true;
}

DataSharingUI::DataSharingUI(content::WebUI* web_ui)
    : UntrustedTopChromeWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIUntrustedDataSharingURL);

  webui::SetupWebUIDataSource(source, kDataSharingResources,
                              IDR_DATA_SHARING_DATA_SHARING_HTML);

  // Allow untrusted mojo resources to be loaded.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src "
      "chrome-untrusted://resources "
      "chrome-untrusted://webui-test "
      "'unsafe-inline' 'self';");

  // Allow images and avatars to be loaded.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc,
      "img-src "
      "https://lh3.google.com "
      "https://lh3.googleusercontent.com "
      "https://www.gstatic.com "
      "'self';");

  // Allow stylesheets to be loaded.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src "
      "chrome-untrusted://theme "
      "chrome-untrusted://resources "
      "'unsafe-inline' "
      "'self';");

  // Allow external network connections to be made.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ConnectSrc,
      "connect-src "
      "https://play.google.com "
      "https://peoplestack-pa.clients6.google.com;");

  // Allow trsuted types to be created.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types "
      "goog#html "
      "lit-html "
      "static-types "
      "webui-test-script;");

  static constexpr webui::LocalizedString kStrings[] = {
      // static messages:
      {"cancel", IDS_DATA_SHARING_CANCEL},
      {"close", IDS_DATA_SHARING_CLOSE},
      {"back", IDS_DATA_SHARING_BACK},
      {"loading", IDS_DATA_SHARING_LOADING},
      {"somethingWrong", IDS_DATA_SHARING_SOMETHING_WRONG},
      {"somethingWrongBody", IDS_DATA_SHARING_SOMETHING_WRONG_BODY},
      {"copyLink", IDS_DATA_SHARING_COPY_LINK},
      {"copyLinkSuccess", IDS_DATA_SHARING_COPY_LINK_SUCCESS},
      {"copyLinkFailed", IDS_DATA_SHARING_COPY_LINK_FAILED},
      {"previewDialogConfirm", IDS_DATA_SHARING_PREVIEW_DIALOG_CONFIRM},
      {"previewDialogConfirm", IDS_DATA_SHARING_PREVIEW_DIALOG_CONFIRM},
      {"tabsInGroup", IDS_DATA_SHARING_PREVIEW_DIALOG_DETAILS_TABS_IN_GROUP},
      {"linkJoinToggle", IDS_DATA_SHARING_LINK_JOIN_TOGGLE},
      {"manageShareWisely", IDS_DATA_SHARING_MANAGE_SHARE_WISELY},
      {"blockLeaveDialogTitle", IDS_DATA_SHARING_BLOCK_LEAVE_DIALOG_TITLE},
      {"blockLeaveDialogConfrim", IDS_DATA_SHARING_BLOCK_LEAVE_DIALOG_CONFIRM},
      {"blockLeaveLearnMore", IDS_DATA_SHARING_BLOCK_LEAVE_LEARN_MORE},
      {"gotIt", IDS_DATA_SHARING_GOT_IT},
      {"joinWarning", IDS_DATA_SHARING_JOIN_WARNING},
      {"ownerStopSharingDialogTitle",
       IDS_DATA_SHARING_OWNER_STOP_SHARING_DIALOG_TITLE},
      {"manageStopSharingOption", IDS_DATA_SHARING_MANAGE_STOP_SHARING_OPTION},
      {"block", IDS_DATA_SHARING_BLOCK},
      {"leaveGroup", IDS_DATA_SHARING_LEAVE_GROUP},
      {"leaveGroupConfirm", IDS_DATA_SHARING_LEAVE_DIALOG_CONFIRM},
      {"leaveDialogTitle", IDS_DATA_SHARING_LEAVE_DIALOG_TITLE},
      {"remove", IDS_DATA_SHARING_REMOVE},
      {"you", IDS_DATA_SHARING_YOU},
      {"owner", IDS_DATA_SHARING_OWNER},
      {"shareGroupBody", IDS_DATA_SHARING_SHARE_GROUP_BODY},
      {"copyInviteLink", IDS_DATA_SHARING_COPY_LINK},
      {"learnMoreJoinFlow", IDS_DATA_SHARING_LEARN_ABOUT_SHARED_GROUPS},
      {"learnMoreSharedTabGroup", IDS_DATA_SHARING_LEARN_ABOUT_SHARED_GROUPS},
      {"tabGroupDetailsTitle", IDS_DATA_SHARING_PREVIEW_DIALOG_DETAILS_TITLE},
      {"peopleWithAccess", IDS_DATA_SHARING_PEOPLE_WITH_ACCESS},
      {"peopleWithAccessSubtitleManageFlow", IDS_DATA_SHARING_LINK_EDIT_ACCESS},
      {"errorDialogContent", IDS_DATA_SHARING_SHARE_ERROR_BODY},
      {"moreOptions", IDS_DATA_SHARING_MORE_OPTIONS},
      {"moreOptionsDescription", IDS_DATA_SHARING_MORE_OPTIONS_DESCRIPTION},

      // dynamic messages:
      {"shareGroupShareAs", IDS_DATA_SHARING_SHARE_GROUP_SHARE_AS},
      {"joinGroupJoinAs", IDS_DATA_SHARING_JOIN_GROUP_JOIN_AS},
      {"memberCountSingular", IDS_DATA_SHARING_MEMBER_COUNT_SINGULAR},
      {"memberCountPlural", IDS_DATA_SHARING_MEMBER_COUNT_PLURAL},
      {"tabCountSingular", IDS_DATA_SHARING_TAB_COUNT_SINGULAR},
      {"tabCountPlural", IDS_DATA_SHARING_TAB_COUNT_PLURAL},
      {"ownerStopSharingDialogBody",
       IDS_DATA_SHARING_OWNER_STOP_SHARING_DIALOG_BODY},
      {"ownerRemoveMemberDialogTitle",
       IDS_DATA_SHARING_OWNER_REMOVE_MEMBER_DIALOG_TITLE},
      {"ownerRemoveMemberDialogBody",
       IDS_DATA_SHARING_OWNER_REMOVE_MEMBER_DIALOG_BODY},
      {"leaveDialogBody", IDS_DATA_SHARING_LEAVE_DIALOG_BODY},
      {"blockDialogTitle", IDS_DATA_SHARING_BLOCK_DIALOG_TITLE},
      {"ownerRemoveMemberDialogBody",
       IDS_DATA_SHARING_OWNER_REMOVE_MEMBER_DIALOG_BODY},
      {"leaveDialogBody", IDS_DATA_SHARING_LEAVE_DIALOG_BODY},
      {"shareGroupTitle", IDS_DATA_SHARING_SHARE_GROUP_TITLE},
      {"previewDialogTitleZero", IDS_DATA_SHARING_PREVIEW_DIALOG_TITLE_ZERO},
      {"previewDialogTitleSingular",
       IDS_DATA_SHARING_PREVIEW_DIALOG_TITLE_SINGULAR},
      {"previewDialogTitlePlural",
       IDS_DATA_SHARING_PREVIEW_DIALOG_TITLE_PLURAL},
      {"previewDialogBody", IDS_DATA_SHARING_PREVIEW_DIALOG_BODY},
      {"manageGroupTitle", IDS_DATA_SHARING_MANAGE_GROUP_TITLE},
      {"groupFull", IDS_DATA_SHARING_GROUP_FULL},
      {"ownerCannotShare", IDS_DATA_SHARING_OWNER_CANNOT_SHARE},
  };
  source->AddLocalizedStrings(kStrings);
}

DataSharingUI::~DataSharingUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(DataSharingUI)

void DataSharingUI::BindInterface(
    mojo::PendingReceiver<data_sharing::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void DataSharingUI::ApiInitComplete() {
  if (delegate_) {
    delegate_->ApiInitComplete();
  }
}

void DataSharingUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
}

void DataSharingUI::CreatePageHandler(
    mojo::PendingRemote<data_sharing::mojom::Page> page,
    mojo::PendingReceiver<data_sharing::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<DataSharingPageHandler>(
      this, std::move(receiver), std::move(page));
}
