// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/dialogs/process_singleton_dialog_linux.h"

#include <string_view>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Shows a system-native dialog (zenity on GNOME, kdialog on KDE, xmessage as
// a last resort) prompting the user about an existing browser session that
// is using their profile directory. Returns true if the user chose to
// relaunch in the existing session, false otherwise (declined, no supported
// tool was found in $PATH, or launching the tool failed).
bool ShowSystemNativeDialog(const std::u16string& message,
                            const std::u16string& relaunch_text) {
  std::string message_utf8 = base::UTF16ToUTF8(message);
  std::string relaunch_utf8 = base::UTF16ToUTF8(relaunch_text);
  std::string product_name_utf8 = l10n_util::GetStringUTF8(IDS_PRODUCT_NAME);
  std::string quit_utf8 =
      l10n_util::GetStringUTF8(IDS_PROFILE_IN_USE_LINUX_QUIT);

  // Prefer the native dialog tool for the current desktop environment.
  auto env = base::Environment::Create();
  auto desktop = env->GetVar("XDG_CURRENT_DESKTOP");
  const bool is_kde =
      desktop.has_value() && desktop->find("KDE") != std::string::npos;

  std::string_view tools[] = {is_kde ? "kdialog" : "zenity",
                              is_kde ? "zenity" : "kdialog", "xmessage"};
  for (std::string_view tool : tools) {
    const base::FilePath tool_path(tool);
    if (!base::ExecutableExistsInPath(env.get(), tool_path.value())) {
      continue;
    }

    base::CommandLine cmd(tool_path);
    if (tool == "zenity") {
      cmd.AppendArg("--question");
      cmd.AppendArg("--title");
      cmd.AppendArg(product_name_utf8);
      cmd.AppendArg("--text");
      cmd.AppendArg(message_utf8);
      cmd.AppendArg("--ok-label");
      cmd.AppendArg(relaunch_utf8);
      cmd.AppendArg("--cancel-label");
      cmd.AppendArg(quit_utf8);
      cmd.AppendArg("--no-wrap");
    } else if (tool == "kdialog") {
      cmd.AppendArg("--title");
      cmd.AppendArg(product_name_utf8);
      cmd.AppendArg("--yesno");
      cmd.AppendArg(message_utf8);
      cmd.AppendArg("--yes-label");
      cmd.AppendArg(relaunch_utf8);
      cmd.AppendArg("--no-label");
      cmd.AppendArg(quit_utf8);
    } else if (tool == "xmessage") {
      cmd.AppendArg("-center");
      cmd.AppendArg("-title");
      cmd.AppendArg(product_name_utf8);
      cmd.AppendArg(message_utf8);
      cmd.AppendArg("-buttons");
      cmd.AppendArg(relaunch_utf8 + ":0," + quit_utf8 + ":1");
    }

    base::LaunchOptions options;
    int exit_code = -1;
    auto process = base::LaunchProcess(cmd, options);
    if (!process.IsValid()) {
      continue;
    }
    process.WaitForExit(&exit_code);
    if (exit_code == 0) {
      return true;
    }
    return false;
  }

  // No system dialog tool is available. Log instructions for manual recovery.
  LOG(ERROR) << "Cannot show profile-in-use dialog: no system dialog tool "
             << "(zenity, kdialog, xmessage) was found. To recover, delete "
             << "the files SingletonLock, SingletonCookie, SingletonSocket "
             << "in your profile directory.";
  return false;
}

}  // namespace

bool ShowProcessSingletonDialog(const std::u16string& message,
                                const std::u16string& relaunch_text) {
  return ShowSystemNativeDialog(message, relaunch_text);
}
