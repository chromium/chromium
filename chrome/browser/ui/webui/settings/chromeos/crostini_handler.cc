// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/crostini_handler.h"

#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_share_path.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
namespace settings {

CrostiniHandler::CrostiniHandler(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {}

CrostiniHandler::~CrostiniHandler() = default;

void CrostiniHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestCrostiniInstallerView",
      base::BindRepeating(&CrostiniHandler::HandleRequestCrostiniInstallerView,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestRemoveCrostini",
      base::BindRepeating(&CrostiniHandler::HandleRequestRemoveCrostini,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getCrostiniSharedPathsDisplayText",
      base::BindRepeating(
          &CrostiniHandler::HandleGetCrostiniSharedPathsDisplayText,
          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "removeCrostiniSharedPath",
      base::BindRepeating(&CrostiniHandler::HandleRemoveCrostiniSharedPath,
                          weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniHandler::HandleRequestCrostiniInstallerView(
    const base::ListValue* args) {
  AllowJavascript();
  ShowCrostiniInstallerView(Profile::FromWebUI(web_ui()),
                            crostini::CrostiniUISurface::kSettings);
}

void CrostiniHandler::HandleRequestRemoveCrostini(const base::ListValue* args) {
  AllowJavascript();
  ShowCrostiniUninstallerView(Profile::FromWebUI(web_ui()),
                              crostini::CrostiniUISurface::kSettings);
}

void CrostiniHandler::HandleGetCrostiniSharedPathsDisplayText(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(2U, args->GetSize());
  std::string callback_id;
  const base::ListValue* paths;
  CHECK(args->GetString(0, &callback_id));
  CHECK(args->GetList(1, &paths));

  base::ListValue texts;
  for (auto it = paths->begin(); it != paths->end(); ++it) {
    texts.AppendString(file_manager::util::GetPathDisplayTextForSettings(
        profile_, it->GetString()));
  }
  ResolveJavascriptCallback(base::Value(callback_id), texts);
}

void CrostiniHandler::HandleRemoveCrostiniSharedPath(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string path;
  CHECK(args->GetString(0, &path));

  crostini::UnsharePath(profile_, crostini::kCrostiniDefaultVmName,
                        base::FilePath(path), base::DoNothing());
}

}  // namespace settings
}  // namespace chromeos
