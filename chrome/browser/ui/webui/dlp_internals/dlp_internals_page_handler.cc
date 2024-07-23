// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/dlp_internals/dlp_internals_page_handler.h"

#include <sys/stat.h>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/dlp_internals/dlp_internals.mojom.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "components/enterprise/data_controls/core/browser/dlp_policy_event.pb.h"
#include "components/enterprise/data_controls/core/browser/rule.h"
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

dlp_internals::mojom::DlpEvent::Restriction EventRestrictionToMojo(
    DlpPolicyEvent::Restriction restriction) {
  switch (restriction) {
    case DlpPolicyEvent_Restriction_UNDEFINED_RESTRICTION:
      return dlp_internals::mojom::DlpEvent::Restriction::kUndefinedRestriction;
    case DlpPolicyEvent_Restriction_CLIPBOARD:
      return dlp_internals::mojom::DlpEvent::Restriction::kClipboard;
    case DlpPolicyEvent_Restriction_SCREENSHOT:
      return dlp_internals::mojom::DlpEvent::Restriction::kScreenshot;
    case DlpPolicyEvent_Restriction_SCREENCAST:
      return dlp_internals::mojom::DlpEvent::Restriction::kScreencast;
    case DlpPolicyEvent_Restriction_PRINTING:
      return dlp_internals::mojom::DlpEvent::Restriction::kPrinting;
    case DlpPolicyEvent_Restriction_EPRIVACY:
      return dlp_internals::mojom::DlpEvent::Restriction::kEprivacy;
    case DlpPolicyEvent_Restriction_FILES:
      return dlp_internals::mojom::DlpEvent::Restriction::kFiles;
  }
}

dlp_internals::mojom::DlpEvent::UserType EventUserTypeToMojo(
    DlpPolicyEvent::UserType restriction) {
  switch (restriction) {
    case DlpPolicyEvent_UserType_REGULAR:
      return dlp_internals::mojom::DlpEvent::UserType::kRegular;
    case DlpPolicyEvent_UserType_MANAGED_GUEST:
      return dlp_internals::mojom::DlpEvent::UserType::kManagedGuest;
    case DlpPolicyEvent_UserType_KIOSK:
      return dlp_internals::mojom::DlpEvent::UserType::kKiosk;
    case DlpPolicyEvent_UserType_UNDEFINED_USER_TYPE:
      return dlp_internals::mojom::DlpEvent::UserType::kUndefinedUserType;
  }
}

dlp_internals::mojom::DlpEvent::Mode EventModeToMojo(
    DlpPolicyEvent::Mode mode) {
  switch (mode) {
    case DlpPolicyEvent_Mode_UNDEFINED_MODE:
      return dlp_internals::mojom::DlpEvent::Mode::kUndefinedMode;
    case DlpPolicyEvent_Mode_BLOCK:
      return dlp_internals::mojom::DlpEvent::Mode::kBlock;
    case DlpPolicyEvent_Mode_REPORT:
      return dlp_internals::mojom::DlpEvent::Mode::kReport;
    case DlpPolicyEvent_Mode_WARN:
      return dlp_internals::mojom::DlpEvent::Mode::kWarn;
    case DlpPolicyEvent_Mode_WARN_PROCEED:
      return dlp_internals::mojom::DlpEvent::Mode::kWarnProceed;
  }
}

dlp_internals::mojom::EventDestination::Component
EventDestinationComponentToMojo(
    DlpPolicyEventDestination::Component component) {
  switch (component) {
    case DlpPolicyEventDestination_Component_UNDEFINED_COMPONENT:
      return dlp_internals::mojom::EventDestination::Component::
          kUndefinedComponent;
    case DlpPolicyEventDestination_Component_ARC:
      return dlp_internals::mojom::EventDestination::Component::kArc;
    case DlpPolicyEventDestination_Component_CROSTINI:
      return dlp_internals::mojom::EventDestination::Component::kCrostini;
    case DlpPolicyEventDestination_Component_PLUGIN_VM:
      return dlp_internals::mojom::EventDestination::Component::kPluginVm;
    case DlpPolicyEventDestination_Component_USB:
      return dlp_internals::mojom::EventDestination::Component::kUsb;
    case DlpPolicyEventDestination_Component_DRIVE:
      return dlp_internals::mojom::EventDestination::Component::kDrive;
    case DlpPolicyEventDestination_Component_ONEDRIVE:
      return dlp_internals::mojom::EventDestination::Component::kOnedrive;
  }
}

dlp_internals::mojom::EventDestinationPtr EventDestinationToMojo(
    DlpPolicyEventDestination destination) {
  auto destination_mojo = dlp_internals::mojom::EventDestination::New();

  if (destination.has_component()) {
    destination_mojo->component =
        EventDestinationComponentToMojo(destination.component());
  }

  if (destination.has_url()) {
    destination_mojo->url_pattern = destination.url();
  }
  return destination_mojo;
}

}  // namespace

DlpInternalsPageHandler::DlpInternalsPageHandler(
    mojo::PendingReceiver<dlp_internals::mojom::PageHandler> receiver,
    Profile* profile)
    : receiver_(this, std::move(receiver)), profile_(profile) {
  DCHECK(profile_);

  auto* rules_manager = DlpRulesManagerFactory::GetForPrimaryProfile();
  auto* reporting_manager =
      rules_manager ? rules_manager->GetReportingManager() : nullptr;
  if (reporting_manager) {
    reporting_observation_.Observe(reporting_manager);
  }
}

DlpInternalsPageHandler::~DlpInternalsPageHandler() = default;

void DlpInternalsPageHandler::GetClipboardDataSource(
    GetClipboardDataSourceCallback callback) {
  auto source = ui::Clipboard::GetForCurrentThread()->GetSource(
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

void DlpInternalsPageHandler::ObserveReporting(
    mojo::PendingRemote<dlp_internals::mojom::ReportingObserver> observer) {
  reporting_observers_.Add(std::move(observer));
}

void DlpInternalsPageHandler::GetFilesDatabaseEntries(
    GetFilesDatabaseEntriesCallback callback) {
  if (!chromeos::DlpClient::Get() || !chromeos::DlpClient::Get()->IsAlive()) {
    std::move(callback).Run({});
    return;
  }
  chromeos::DlpClient::Get()->GetDatabaseEntries(
      base::BindOnce(&DlpInternalsPageHandler::ProcessDatabaseEntries,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DlpInternalsPageHandler::GetFileInode(const std::string& file_name,
                                           GetFileInodeCallback callback) {
  base::FilePath downloads_path;
  base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS_SAFE, &downloads_path);
  auto file_path = downloads_path.Append(file_name);

  struct stat file_stats;
  if (stat(file_path.value().c_str(), &file_stats) != 0) {
    std::move(callback).Run(0);
    return;
  }

  std::move(callback).Run(file_stats.st_ino);
}

void DlpInternalsPageHandler::OnReportEvent(DlpPolicyEvent event) {
  dlp_internals::mojom::DlpEventPtr event_mojo =
      dlp_internals::mojom::DlpEvent::New();
  if (event.has_source() && event.source().has_url()) {
    event_mojo->source_pattern = event.source().url();
  }

  if (event.has_destination()) {
    event_mojo->destination = EventDestinationToMojo(event.destination());
  }

  if (event.has_restriction()) {
    event_mojo->restriction = EventRestrictionToMojo(event.restriction());
  }

  if (event.mode()) {
    event_mojo->mode = EventModeToMojo(event.mode());
  }

  if (event.has_timestamp_micro()) {
    event_mojo->timestamp_micro = event.timestamp_micro();
  }

  if (event.has_user_type()) {
    event_mojo->user_type = EventUserTypeToMojo(event.user_type());
  }

  if (event.has_content_name()) {
    event_mojo->content_name = event.content_name();
  }

  if (event.has_triggered_rule_name()) {
    event_mojo->triggered_rule_name = event.triggered_rule_name();
  }

  if (event.has_triggered_rule_id()) {
    event_mojo->triggered_rule_id = event.triggered_rule_id();
  }

  for (auto& observer : reporting_observers_) {
    observer->OnReportEvent(event_mojo.Clone());
  }
}

void DlpInternalsPageHandler::ProcessDatabaseEntries(
    GetFilesDatabaseEntriesCallback callback,
    ::dlp::GetDatabaseEntriesResponse response_proto) {
  std::vector<dlp_internals::mojom::FileDatabaseEntryPtr> database_entries;
  for (const auto& file_entry : response_proto.files_entries()) {
    database_entries.push_back(dlp_internals::mojom::FileDatabaseEntry::New(
        file_entry.inode(), file_entry.crtime(), file_entry.source_url(),
        file_entry.referrer_url()));
  }
  std::move(callback).Run(std::move(database_entries));
}

}  // namespace policy
