// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/network_ui/network_logs_message_handler.h"

#include <iostream>

#include "base/files/file_util.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/file_manager/filesystem_api_util.h"
#include "chrome/browser/ash/system_logs/debug_log_writer.h"
#include "chrome/browser/ash/system_logs/system_logs_writer.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

base::FilePath GetDownloadsDirectory(content::WebUI* web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  const DownloadPrefs* const prefs = DownloadPrefs::FromBrowserContext(profile);
  base::FilePath path = prefs->DownloadPath();
  if (file_manager::util::IsUnderNonNativeLocalPath(profile, path))
    path = prefs->GetDefaultDownloadDirectoryForProfile();
  return path;
}

std::string GetJsonPolicies(content::WebUI* web_ui) {
  return policy::PolicyConversions(
             std::make_unique<policy::ChromePolicyConversionsClient>(
                 web_ui->GetWebContents()->GetBrowserContext()))
      .ToJSON();
}

bool WriteTimestampedFile(const base::FilePath& file_path,
                          const std::string& contents) {
  base::FilePath timestamped_file_path =
      logging::GenerateTimestampedName(file_path, base::Time::Now());
  return base::WriteFile(timestamped_file_path, contents);
}

bool GetBoolOrFalse(const base::Value::Dict& dict, const char* keyname) {
  const auto key = dict.FindBool(keyname);
  return key && *key;
}

}  // namespace

NetworkLogsMessageHandler::NetworkLogsMessageHandler() = default;

NetworkLogsMessageHandler::~NetworkLogsMessageHandler() = default;

void NetworkLogsMessageHandler::RegisterMessages() {
  out_dir_ = GetDownloadsDirectory(web_ui());
  web_ui()->RegisterMessageCallback(
      "storeLogs", base::BindRepeating(&NetworkLogsMessageHandler::OnStoreLogs,
                                       base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setShillDebugging",
      base::BindRepeating(&NetworkLogsMessageHandler::OnSetShillDebugging,
                          base::Unretained(this)));
}

void NetworkLogsMessageHandler::Respond(const std::string& callback_id,
                                        const std::string& result,
                                        bool is_error) {
  base::Value::List response;
  response.Append(result);
  response.Append(is_error);
  ResolveJavascriptCallback(base::Value(callback_id), response);
}

void NetworkLogsMessageHandler::OnStoreLogs(const base::Value::List& list) {
  CHECK_EQ(2u, list.size());
  std::string callback_id = list[0].GetString();
  const base::Value::Dict& options = list[1].GetDict();
  AllowJavascript();

  if (GetBoolOrFalse(options, "systemLogs")) {
    bool scrub_data = GetBoolOrFalse(options, "filterPII");
    system_logs_writer::WriteSystemLogs(
        Profile::FromWebUI(web_ui()), out_dir_, scrub_data,
        base::BindOnce(&NetworkLogsMessageHandler::OnWriteSystemLogs,
                       weak_factory_.GetWeakPtr(), callback_id,
                       options.Clone()));
  } else {
    MaybeWriteDebugLogs(callback_id, options.Clone());
  }
}

void NetworkLogsMessageHandler::OnWriteSystemLogs(
    const std::string& callback_id,
    base::Value::Dict&& options,
    std::optional<base::FilePath> syslogs_path) {
  if (!syslogs_path) {
    Respond(callback_id, "Error writing system logs file.", /*is_error=*/true);
    return;
  }
  MaybeWriteDebugLogs(callback_id, std::move(options));
}

void NetworkLogsMessageHandler::MaybeWriteDebugLogs(
    const std::string& callback_id,
    base::Value::Dict&& options) {
  if (GetBoolOrFalse(options, "debugLogs")) {
    if (!base::SysInfo::IsRunningOnChromeOS()) {
      Respond(callback_id, "Debug logs unavailable on Linux build.",
              /*is_error=*/true);
      return;
    }
    bool include_chrome = GetBoolOrFalse(options, "chromeLogs");
    debug_log_writer::StoreLogs(
        out_dir_, include_chrome,
        base::BindOnce(&NetworkLogsMessageHandler::OnWriteDebugLogs,
                       weak_factory_.GetWeakPtr(), callback_id,
                       std::move(options)));
  } else {
    MaybeWritePolicies(callback_id, std::move(options));
  }
}

void NetworkLogsMessageHandler::OnWriteDebugLogs(
    const std::string& callback_id,
    base::Value::Dict&& options,
    std::optional<base::FilePath> logs_path) {
  if (!logs_path) {
    Respond(callback_id, "Error writing debug logs.", /*is_error=*/true);
    return;
  }
  MaybeWritePolicies(callback_id, std::move(options));
}

void NetworkLogsMessageHandler::MaybeWritePolicies(
    const std::string& callback_id,
    base::Value::Dict&& options) {
  if (GetBoolOrFalse(options, "policies")) {
    std::string json_policies = GetJsonPolicies(web_ui());
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(WriteTimestampedFile, out_dir_.Append("policies.json"),
                       json_policies),
        base::BindOnce(&NetworkLogsMessageHandler::OnWritePolicies,
                       weak_factory_.GetWeakPtr(), callback_id));
  } else {
    OnWriteSystemLogsCompleted(callback_id);
  }
}

void NetworkLogsMessageHandler::OnWritePolicies(const std::string& callback_id,
                                                bool result) {
  if (!result) {
    Respond(callback_id, "Error writing policies.", /*is_error=*/true);
    return;
  }
  OnWriteSystemLogsCompleted(callback_id);
}

void NetworkLogsMessageHandler::OnWriteSystemLogsCompleted(
    const std::string& callback_id) {
  Respond(callback_id,
          l10n_util::GetStringUTF8(IDS_NETWORK_UI_NETWORK_LOGS_SUCCESS),
          /*is_error=*/false);
}

void NetworkLogsMessageHandler::OnSetShillDebugging(
    const base::Value::List& list) {
  CHECK_EQ(2u, list.size());
  std::string callback_id = list[0].GetString();
  std::string subsystem = list[1].GetString();
  AllowJavascript();
  DebugDaemonClient::Get()->SetDebugMode(
      subsystem,
      base::BindOnce(&NetworkLogsMessageHandler::OnSetShillDebuggingCompleted,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void NetworkLogsMessageHandler::OnSetShillDebuggingCompleted(
    const std::string& callback_id,
    bool succeeded) {
  Respond(callback_id, /*result=*/"", !succeeded);
}

}  // namespace ash
