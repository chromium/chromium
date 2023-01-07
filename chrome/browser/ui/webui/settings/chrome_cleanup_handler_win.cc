// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chrome_cleanup_handler_win.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "chrome/grit/generated_resources.h"
#include "components/component_updater/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/l10n/l10n_util.h"

using safe_browsing::ChromeCleanerController;

namespace settings {

namespace {

// Returns a base::Value::List containing a copy of the file paths stored in
// |files|.
base::Value::List GetFilesAsListStorage(const std::set<base::FilePath>& files) {
  base::Value::List value;
  for (const base::FilePath& path : files) {
    base::Value::Dict item;
    item.Set("dirname", path.DirName().AsEndingWithSeparator().AsUTF8Unsafe());
    item.Set("basename", path.BaseName().AsUTF8Unsafe());
    value.Append(std::move(item));
  }
  return value;
}

// Returns a base::Value::List containing a copy of the strings stored in
// |string_set|.
base::Value::List GetStringSetAsListStorage(
    const std::set<std::wstring>& string_set) {
  base::Value::List value;
  for (const std::wstring& string : string_set)
    value.Append(base::AsString16(string));

  return value;
}

base::Value::Dict GetScannerResultsAsDictionary(
    const safe_browsing::ChromeCleanerScannerResults& scanner_results,
    Profile* profile) {
  base::Value::Dict value;
  value.Set("files", GetFilesAsListStorage(scanner_results.files_to_delete()));
  value.Set("registryKeys",
            GetStringSetAsListStorage(scanner_results.registry_keys()));
  return value;
}

std::string IdleReasonToString(
    ChromeCleanerController::IdleReason idle_reason) {
  switch (idle_reason) {
    case ChromeCleanerController::IdleReason::kInitial:
      return "initial";
    case ChromeCleanerController::IdleReason::kReporterFoundNothing:
      return "reporter_found_nothing";
    case ChromeCleanerController::IdleReason::kReporterFailed:
      return "reporter_failed";
    case ChromeCleanerController::IdleReason::kScanningFoundNothing:
      return "scanning_found_nothing";
    case ChromeCleanerController::IdleReason::kScanningFailed:
      return "scanning_failed";
    case ChromeCleanerController::IdleReason::kConnectionLost:
      return "connection_lost";
    case ChromeCleanerController::IdleReason::kUserDeclinedCleanup:
      return "user_declined_cleanup";
    case ChromeCleanerController::IdleReason::kCleaningFailed:
      return "cleaning_failed";
    case ChromeCleanerController::IdleReason::kCleaningSucceeded:
      return "cleaning_succeeded";
    case ChromeCleanerController::IdleReason::kCleanerDownloadFailed:
      return "cleaner_download_failed";
  }
  NOTREACHED();
  return "";
}

}  // namespace

ChromeCleanupHandler::ChromeCleanupHandler(Profile* profile)
    : controller_(ChromeCleanerController::GetInstance()), profile_(profile) {}

ChromeCleanupHandler::~ChromeCleanupHandler() {
  controller_->RemoveObserver(this);
}

void ChromeCleanupHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "registerChromeCleanerObserver",
      base::BindRepeating(
          &ChromeCleanupHandler::HandleRegisterChromeCleanerObserver,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "startScanning",
      base::BindRepeating(&ChromeCleanupHandler::HandleStartScanning,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "restartComputer",
      base::BindRepeating(&ChromeCleanupHandler::HandleRestartComputer,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "startCleanup",
      base::BindRepeating(&ChromeCleanupHandler::HandleStartCleanup,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "notifyShowDetails",
      base::BindRepeating(&ChromeCleanupHandler::HandleNotifyShowDetails,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "notifyChromeCleanupLearnMoreClicked",
      base::BindRepeating(
          &ChromeCleanupHandler::HandleNotifyChromeCleanupLearnMoreClicked,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getMoreItemsPluralString",
      base::BindRepeating(&ChromeCleanupHandler::HandleGetMoreItemsPluralString,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getItemsToRemovePluralString",
      base::BindRepeating(
          &ChromeCleanupHandler::HandleGetItemsToRemovePluralString,
          base::Unretained(this)));
}

void ChromeCleanupHandler::OnJavascriptAllowed() {
  controller_->AddObserver(this);
}

void ChromeCleanupHandler::OnJavascriptDisallowed() {
  controller_->RemoveObserver(this);
}

void ChromeCleanupHandler::OnIdle(
    ChromeCleanerController::IdleReason idle_reason) {
  FireWebUIListener("chrome-cleanup-on-idle",
                    base::Value(IdleReasonToString(idle_reason)));
}

void ChromeCleanupHandler::OnScanning() {
  FireWebUIListener("chrome-cleanup-on-scanning");
}

void ChromeCleanupHandler::OnReporterRunning() {
  FireWebUIListener("chrome-cleanup-on-reporter-running");
}

void ChromeCleanupHandler::OnInfected(
    bool is_powered_by_partner,
    const safe_browsing::ChromeCleanerScannerResults& scanner_results) {
  FireWebUIListener("chrome-cleanup-on-infected",
                    base::Value(is_powered_by_partner),
                    GetScannerResultsAsDictionary(scanner_results, profile_));
}

void ChromeCleanupHandler::OnCleaning(
    bool is_powered_by_partner,
    const safe_browsing::ChromeCleanerScannerResults& scanner_results) {
  FireWebUIListener("chrome-cleanup-on-cleaning",
                    base::Value(is_powered_by_partner),
                    GetScannerResultsAsDictionary(scanner_results, profile_));
}

void ChromeCleanupHandler::OnRebootRequired() {
  FireWebUIListener("chrome-cleanup-on-reboot-required");
}

void ChromeCleanupHandler::HandleRegisterChromeCleanerObserver(
    const base::Value::List& args) {
  DCHECK_EQ(0U, args.size());

  base::RecordAction(
      base::UserMetricsAction("SoftwareReporter.CleanupWebui_Shown"));
  AllowJavascript();

  FireWebUIListener("chrome-cleanup-enabled-change",
                    base::Value(controller_->IsAllowedByPolicy()));
}

void ChromeCleanupHandler::HandleStartScanning(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  bool allow_logs_upload = false;
  if (args[0].is_bool())
    allow_logs_upload = args[0].GetBool();

  // If this operation is not allowed the UI should be disabled.
  CHECK(controller_->IsAllowedByPolicy());

  // The state is propagated to all open tabs and should be consistent.
  DCHECK_EQ(controller_->logs_enabled(profile_), allow_logs_upload);

  controller_->RequestUserInitiatedScan(profile_);

  base::RecordAction(
      base::UserMetricsAction("SoftwareReporter.CleanupWebui_StartScanning"));
}

void ChromeCleanupHandler::HandleRestartComputer(
    const base::Value::List& args) {
  DCHECK_EQ(0U, args.size());

  base::RecordAction(
      base::UserMetricsAction("SoftwareReporter.CleanupWebui_RestartComputer"));

  controller_->Reboot();
}

void ChromeCleanupHandler::HandleStartCleanup(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  bool allow_logs_upload = false;
  if (args[0].is_bool())
    allow_logs_upload = args[0].GetBool();

  // The state is propagated to all open tabs and should be consistent.
  DCHECK_EQ(controller_->logs_enabled(profile_), allow_logs_upload);

  base::RecordAction(
      base::UserMetricsAction("SoftwareReporter.CleanupWebui_StartCleanup"));

  controller_->ReplyWithUserResponse(
      profile_,
      allow_logs_upload
          ? ChromeCleanerController::UserResponse::kAcceptedWithLogs
          : ChromeCleanerController::UserResponse::kAcceptedWithoutLogs);
}

void ChromeCleanupHandler::HandleNotifyShowDetails(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  bool details_section_visible = false;
  if (args[0].is_bool())
    details_section_visible = args[0].GetBool();

  if (details_section_visible) {
    base::RecordAction(
        base::UserMetricsAction("SoftwareReporter.CleanupWebui_ShowDetails"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("SoftwareReporter.CleanupWebui_HideDetails"));
  }
}

void ChromeCleanupHandler::HandleNotifyChromeCleanupLearnMoreClicked(
    const base::Value::List& args) {
  CHECK_EQ(0U, args.size());

  base::RecordAction(
      base::UserMetricsAction("SoftwareReporter.CleanupWebui_LearnMore"));
}

void ChromeCleanupHandler::HandleGetMoreItemsPluralString(
    const base::Value::List& args) {
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  GetPluralString(IDS_SETTINGS_RESET_CLEANUP_DETAILS_MORE, args);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

void ChromeCleanupHandler::HandleGetItemsToRemovePluralString(
    const base::Value::List& args) {
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  GetPluralString(IDS_SETTINGS_RESET_CLEANUP_DETAILS_ITEMS_TO_BE_REMOVED, args);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

void ChromeCleanupHandler::GetPluralString(int id,
                                           const base::Value::List& args) {
  const auto& list = args;
  CHECK_EQ(2U, list.size());

  std::string callback_id = list[0].GetString();

  int num_items = list[1].GetIfInt().value_or(0);

  const std::u16string plural_string =
      num_items > 0 ? l10n_util::GetPluralStringFUTF16(id, num_items)
                    : std::u16string();
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(plural_string));
}

}  // namespace settings
