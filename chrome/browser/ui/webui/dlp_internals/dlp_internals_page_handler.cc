// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/dlp_internals/dlp_internals_page_handler.h"

#include "base/check.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/dlp_internals/dlp_internals.mojom.h"
#include "components/enterprise/data_controls/rule.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/clipboard/clipboard.h"

namespace policy {

namespace {
dlp_internals::mojom::Level DlpLevelToMojo(data_controls::Rule::Level level) {
  switch (level) {
    case data_controls::Rule::Level::kNotSet:
      return dlp_internals::mojom::Level::kNotSet;
    case data_controls::Rule::Level::kReport:
      return dlp_internals::mojom::Level::kReport;
    case data_controls::Rule::Level::kWarn:
      return dlp_internals::mojom::Level::kWarn;
    case data_controls::Rule::Level::kBlock:
      return dlp_internals::mojom::Level::kBlock;
    case data_controls::Rule::Level::kAllow:
      return dlp_internals::mojom::Level::kAllow;
  }
}

dlp_internals::mojom::ContentRestriction ContentRestrictionToMojo(
    DlpContentRestriction restriction) {
  switch (restriction) {
    case DlpContentRestriction::kScreenshot:
      return dlp_internals::mojom::ContentRestriction::kScreenshot;
    case DlpContentRestriction::kPrivacyScreen:
      return dlp_internals::mojom::ContentRestriction::kPrivacyScreen;
    case DlpContentRestriction::kPrint:
      return dlp_internals::mojom::ContentRestriction::kPrint;
    case DlpContentRestriction::kScreenShare:
      return dlp_internals::mojom::ContentRestriction::kScreenShare;
  }
}

std::vector<dlp_internals::mojom::ContentRestrictionInfoPtr>
RestrictionSetToMojo(DlpContentRestrictionSet restriction_set) {
  std::vector<dlp_internals::mojom::ContentRestrictionInfoPtr>
      restrictions_mojo_arr;

  // An array of all content restrictions. Please keep it up to date with
  // DlpContentRestriction enum.
  std::array restrictions = {
      DlpContentRestriction::kScreenshot, DlpContentRestriction::kPrivacyScreen,
      DlpContentRestriction::kPrint, DlpContentRestriction::kScreenShare};
  for (const auto& restriction : restrictions) {
    auto lvl_and_url = restriction_set.GetRestrictionLevelAndUrl(restriction);
    restrictions_mojo_arr.push_back(
        dlp_internals::mojom::ContentRestrictionInfo::New(
            ContentRestrictionToMojo(restriction),
            DlpLevelToMojo(lvl_and_url.level), lvl_and_url.url));
  }
  return restrictions_mojo_arr;
}

std::vector<dlp_internals::mojom::RenderFrameHostInfoPtr> RfhInfoToMojo(
    std::vector<DlpContentTabHelper::RfhInfo> rfh_info_vector) {
  std::vector<dlp_internals::mojom::RenderFrameHostInfoPtr> rfh_info_mojo_arr;
  for (const auto& rfh_info : rfh_info_vector) {
    DCHECK(rfh_info.first);
    rfh_info_mojo_arr.push_back(dlp_internals::mojom::RenderFrameHostInfo::New(
        rfh_info.first->GetLastCommittedURL(),
        RestrictionSetToMojo(rfh_info.second)));
  }
  return rfh_info_mojo_arr;
}

}  // namespace

DlpInternalsPageHandler::DlpInternalsPageHandler(
    mojo::PendingReceiver<dlp_internals::mojom::PageHandler> receiver,
    Profile* profile)
    : receiver_(this, std::move(receiver)), profile_(profile) {
  DCHECK(profile_);
}

DlpInternalsPageHandler::~DlpInternalsPageHandler() = default;

void DlpInternalsPageHandler::GetClipboardDataSource(
    GetClipboardDataSourceCallback callback) {
  const auto* source = ui::Clipboard::GetForCurrentThread()->GetSource(
      ui::ClipboardBuffer::kCopyPaste);
  if (!source) {
    std::move(callback).Run(std::move(nullptr));
    return;
  }

  auto mojom_source = dlp_internals::mojom::DataTransferEndpoint::New();
  switch (source->type()) {
    case ui::EndpointType::kDefault:
      mojom_source->type = dlp_internals::mojom::EndpointType::kDefault;
      break;

    case ui::EndpointType::kUrl:
      mojom_source->type = dlp_internals::mojom::EndpointType::kUrl;
      break;

    case ui::EndpointType::kClipboardHistory:
      mojom_source->type =
          dlp_internals::mojom::EndpointType::kClipboardHistory;
      break;

    case ui::EndpointType::kUnknownVm:
      mojom_source->type = dlp_internals::mojom::EndpointType::kUnknownVm;
      break;

    case ui::EndpointType::kArc:
      mojom_source->type = dlp_internals::mojom::EndpointType::kArc;
      break;

    case ui::EndpointType::kBorealis:
      mojom_source->type = dlp_internals::mojom::EndpointType::kBorealis;
      break;

    case ui::EndpointType::kCrostini:
      mojom_source->type = dlp_internals::mojom::EndpointType::kCrostini;
      break;

    case ui::EndpointType::kPluginVm:
      mojom_source->type = dlp_internals::mojom::EndpointType::kPluginVm;
      break;

    case ui::EndpointType::kLacros:
      mojom_source->type = dlp_internals::mojom::EndpointType::kLacros;
      break;
  }

  if (source->IsUrlType()) {
    mojom_source->url = *source->GetURL();
  }

  std::move(callback).Run(std::move(mojom_source));
}

void DlpInternalsPageHandler::GetContentRestrictionsInfo(
    GetContentRestrictionsInfoCallback callback) {
  auto* content_manager = DlpContentManager::Get();
  if (!content_manager) {
    std::move(callback).Run({});
  }

  auto info_vector = content_manager->GetWebContentsInfo();
  std::vector<dlp_internals::mojom::WebContentsInfoPtr> info_mojo_array;

  for (const auto& web_contents_info : info_vector) {
    DCHECK(web_contents_info.web_contents);
    info_mojo_array.push_back(dlp_internals::mojom::WebContentsInfo::New(
        web_contents_info.web_contents->GetLastCommittedURL(),
        RestrictionSetToMojo(web_contents_info.restriction_set),
        RfhInfoToMojo(web_contents_info.rfh_info_vector)));
  }
  std::move(callback).Run(std::move(info_mojo_array));
}

}  // namespace policy
