// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_shortcuts_menu.h"

#include "base/check.h"
#include "base/notreached.h"
#include "build/build_config.h"

namespace web_app {

#if !BUILDFLAG(IS_WIN)
bool ShouldRegisterShortcutsMenuWithOs() {
  return false;
}

void RegisterShortcutsMenuWithOs(
    const webapps::AppId& app_id,
    const base::FilePath& profile_path,
    const base::FilePath& shortcut_data_dir,
    const std::vector<WebAppShortcutsMenuItemInfo>& shortcuts_menu_item_infos,
    const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps,
    RegisterShortcutsMenuCallback callback) {
  NOTIMPLEMENTED();
  DCHECK(ShouldRegisterShortcutsMenuWithOs());
  std::move(callback).Run(Result::kOk);
}

bool UnregisterShortcutsMenuWithOs(const webapps::AppId& app_id,
                                   const base::FilePath& profile_path,
                                   RegisterShortcutsMenuCallback callback) {
  NOTIMPLEMENTED();
  DCHECK(ShouldRegisterShortcutsMenuWithOs());

  std::move(callback).Run(Result::kOk);
  return true;
}
#endif  // !BUILDFLAG(IS_WIN)

}  // namespace web_app
