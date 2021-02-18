// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/network_logs_message_handler.h"

#include <iostream>

#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/system_logs/debug_log_writer.h"
#include "chrome/browser/ash/system_logs/system_logs_writer.h"
#include "chrome/browser/chromeos/file_manager/filesystem_api_util.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

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
  auto client = std::make_unique<policy::ChromePolicyConversionsClient>(
      web_ui->GetWebContents()->GetBrowserContext());
  return policy::DictionaryPolicyConversions(std::move(client)).ToJSON();
}

bool WriteTimestampedFile(const base::FilePath& file_path,
                          const std::string& contents) {
  base::FilePath timestamped_file_path =
      logging::GenerateTimestampedName(file_path, base::Time::Now());
  int bytes_written =
      base::WriteFile(timestamped_file_path, contents.data(), contents.size());
  return bytes_written > 0;
}

bool GetBoolOrFalse(const base::Value* dict, const char* keyname) {
  const base::Value* key = dict->FindKey(keyname);
  return key && key->GetBool();
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
  base::Value response(base::Value::Type::LIST);
  response.Append(result);
  response.Append(is_error);
  ResolveJavascriptCallback(base::Value(callback_id), response);
}

void NetworkLogsMessageHandler::OnStoreLogs(const base::ListValue* list) {
  CHECK_EQ(2u, list->GetSize());
  std::string callback_id;
  CHECK(list->GetString(0, &callback_id));
  const base::Value* options;
  CHECK(list->Get(1, &options));
  AllowJavascript();

  if (GetBoolOrFalse(options, "systemLogs")) {
    bool scrub_data = GetBoolOrFalse(options, "filterPII");
    chromeos::system_logs_writer::WriteSystemLogs(
        out_dir_, scrub_data,
        base::BindOnce(&NetworkLogsMessageHandler::OnWriteSystemLogs,
                       weak_factory_.GetWeakPtr(), callback_id,
                       options->Clone()));
  } else {
    MaybeWriteDebugLogs(callback_id, options->Clone());
  }
}

void NetworkLogsMessageHandler::OnWriteSystemLogs(
    const std::string& callback_id,
    base::Value&& options,
    base::Optional<base::FilePath> syslogs_path) {
  if (!syslogs_path) {
    Respond(callback_id, "Error writing system logs file.", /*is_error=*/true);
    return;
  }
  MaybeWriteDebugLogs(callback_id, std::move(options));
}

void NetworkLogsMessageHandler::MaybeWriteDebugLogs(
    const std::string& callback_id,
    base::Value&& options) {
  if (GetBoolOrFalse(&options, "debugLogs")) {
    if (!base::SysInfo::IsRunningOnChromeOS()) {
      Respond(callback_id, "Debug logs unavailable on Linux build.",
              /*is_error=*/true);
      return;
    }
    bool include_chrome = GetBoolOrFalse(&options, "chromeLogs");
    chromeos::debug_log_writer::StoreLogs(
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
    base::Value&& options,
    base::Optional<base::FilePath> logs_path) {
  if (!logs_path) {
    Respond(callback_id, "Error writing debug logs.", /*is_error=*/true);
    return;
  }
  MaybeWritePolicies(callback_id, std::move(options));
}

void NetworkLogsMessageHandler::MaybeWritePolicies(
    const std::string& callback_id,
    base::Value&& options) {
  if (GetBoolOrFalse(&options, "policies")) {
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
    const base::ListValue* list) {
  CHECK_EQ(2u, list->GetSize());
  std::string callback_id, subsystem;
  CHECK(list->GetString(0, &callback_id));
  CHECK(list->GetString(1, &subsystem));
  AllowJavascript();
  chromeos::DBusThreadManager::Get()->GetDebugDaemonClient()->SetDebugMode(
      subsystem,
      base::BindOnce(&NetworkLogsMessageHandler::OnSetShillDebuggingCompleted,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void NetworkLogsMessageHandler::OnSetShillDebuggingCompleted(
    const std::string& callback_id,
    bool succeeded) {
  Respond(callback_id, /*result=*/"", !succeeded);
}

}  // namespace chromeos
