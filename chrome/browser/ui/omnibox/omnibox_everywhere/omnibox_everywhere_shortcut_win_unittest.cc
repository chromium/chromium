// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_shortcut_win.h"

// clang-format off
#include <shlobj.h>  // Must be before propkey.
// clang-format on

#include <propkey.h>
#include <propsys.h>
#include <shellapi.h>
#include <wrl/client.h>

#include <optional>
#include <string>

#include "base/base_paths_win.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "base/threading/sequence_bound.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/shortcut.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"
#include "ui/views/win/hwnd_util.h"

namespace omnibox_everywhere {

class OmniboxEverywhereShortcutWinTest : public ChromeViewsTestBase {
 public:
  OmniboxEverywhereShortcutWinTest() = default;
  ~OmniboxEverywhereShortcutWinTest() override = default;
};

TEST_F(OmniboxEverywhereShortcutWinTest, GetAppUserModelIdAndIconPath) {
  std::wstring app_id = GetAppUserModelId();
  EXPECT_FALSE(app_id.empty());
  EXPECT_NE(app_id.find(L"app_search_in_chrome"), std::wstring::npos);

  base::FilePath icon_path = GetIconFilePath();
  EXPECT_FALSE(icon_path.empty());
  EXPECT_EQ(icon_path.BaseName().value(),
            FILE_PATH_LITERAL("SearchInChrome.ico"));
}

TEST_F(OmniboxEverywhereShortcutWinTest, EnsureIconPersisted) {
  base::ScopedTempDir user_data_dir;
  ASSERT_TRUE(user_data_dir.CreateUniqueTempDir());
  base::ScopedPathOverride user_data_override(chrome::DIR_USER_DATA,
                                              user_data_dir.GetPath());

  OmniboxEverywhereShortcutHelperWin helper;
  EXPECT_TRUE(helper.EnsureIconPersisted());
  base::FilePath icon_path = GetIconFilePath();
  EXPECT_TRUE(base::PathExists(icon_path));

  std::optional<int64_t> file_size = base::GetFileSize(icon_path);
  ASSERT_TRUE(file_size.has_value());
  EXPECT_GT(*file_size, 0);

  // Calling EnsureIconPersisted again when the file already exists succeeds.
  EXPECT_TRUE(helper.EnsureIconPersisted());
}

TEST_F(OmniboxEverywhereShortcutWinTest, CreateStartMenuShortcut) {
  base::ScopedTempDir user_data_dir;
  ASSERT_TRUE(user_data_dir.CreateUniqueTempDir());
  base::ScopedPathOverride user_data_override(chrome::DIR_USER_DATA,
                                              user_data_dir.GetPath());

  base::ScopedTempDir start_menu_dir;
  ASSERT_TRUE(start_menu_dir.CreateUniqueTempDir());
  base::ScopedPathOverride start_menu_override(base::DIR_START_MENU,
                                               start_menu_dir.GetPath());

  OmniboxEverywhereShortcutHelperWin helper;
  EXPECT_TRUE(helper.CreateStartMenuShortcut());

  base::FilePath shortcut_path = start_menu_dir.GetPath().Append(
      base::StrCat({base::UTF16ToWide(l10n_util::GetStringUTF16(
                        IDS_SETTINGS_OMNIBOX_EVERYWHERE_TITLE)),
                    L".lnk"}));
  EXPECT_TRUE(base::PathExists(shortcut_path));

  base::win::ShortcutProperties properties;
  EXPECT_TRUE(base::win::ResolveShortcutProperties(
      shortcut_path,
      base::win::ShortcutProperties::PROPERTIES_TARGET |
          base::win::ShortcutProperties::PROPERTIES_ARGUMENTS |
          base::win::ShortcutProperties::PROPERTIES_ICON |
          base::win::ShortcutProperties::PROPERTIES_APP_ID,
      &properties));

  EXPECT_NE(
      properties.target.value().find(FILE_PATH_LITERAL("chrome_proxy.exe")),
      std::wstring::npos);
  EXPECT_NE(properties.arguments.find(L"--omnibox-everywhere"),
            std::wstring::npos);
  EXPECT_EQ(properties.app_id, GetAppUserModelId());
  EXPECT_EQ(properties.icon, GetIconFilePath());
}

TEST_F(OmniboxEverywhereShortcutWinTest, SequenceBoundHelper) {
  base::ScopedTempDir user_data_dir;
  ASSERT_TRUE(user_data_dir.CreateUniqueTempDir());
  base::ScopedPathOverride user_data_override(chrome::DIR_USER_DATA,
                                              user_data_dir.GetPath());

  base::ScopedTempDir start_menu_dir;
  ASSERT_TRUE(start_menu_dir.CreateUniqueTempDir());
  base::ScopedPathOverride start_menu_override(base::DIR_START_MENU,
                                               start_menu_dir.GetPath());

  base::SequenceBound<OmniboxEverywhereShortcutHelperWin> helper(
      base::ThreadPool::CreateCOMSTATaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));

  base::test::TestFuture<bool> future;
  helper.AsyncCall(&OmniboxEverywhereShortcutHelperWin::CreateStartMenuShortcut)
      .Then(future.GetCallback());
  EXPECT_TRUE(future.Get());

  base::FilePath shortcut_path = start_menu_dir.GetPath().Append(
      base::StrCat({base::UTF16ToWide(l10n_util::GetStringUTF16(
                        IDS_SETTINGS_OMNIBOX_EVERYWHERE_TITLE)),
                    L".lnk"}));
  EXPECT_TRUE(base::PathExists(shortcut_path));
}

TEST_F(OmniboxEverywhereShortcutWinTest, SetWindowPropertiesNullHwndSafe) {
  // Verifies that calling SetWindowProperties with a null HWND
  // does not crash in either ephemeral or persistent mode.
  SetWindowProperties(nullptr, /*is_ephemeral=*/true);
  SetWindowProperties(nullptr, /*is_ephemeral=*/false);
}

TEST_F(OmniboxEverywhereShortcutWinTest, SetWindowPropertiesEphemeralMode) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                       views::Widget::InitParams::TYPE_WINDOW);
  HWND hwnd = views::HWNDForWidget(widget.get());
  ASSERT_NE(hwnd, nullptr);

  SetWindowProperties(hwnd, /*is_ephemeral=*/true);

  Microsoft::WRL::ComPtr<IPropertyStore> pps;
  ASSERT_HRESULT_SUCCEEDED(
      SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&pps)));

  base::win::ScopedPropVariant pv;
  ASSERT_HRESULT_SUCCEEDED(
      pps->GetValue(PKEY_AppUserModel_PreventPinning, pv.Receive()));
  EXPECT_EQ(pv.get().vt, VT_BOOL);
  EXPECT_EQ(pv.get().boolVal, VARIANT_TRUE);

  widget->CloseNow();
}

TEST_F(OmniboxEverywhereShortcutWinTest, SetWindowPropertiesPersistentMode) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                       views::Widget::InitParams::TYPE_WINDOW);
  HWND hwnd = views::HWNDForWidget(widget.get());
  ASSERT_NE(hwnd, nullptr);

  SetWindowProperties(hwnd, /*is_ephemeral=*/false);

  Microsoft::WRL::ComPtr<IPropertyStore> pps;
  ASSERT_HRESULT_SUCCEEDED(
      SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&pps)));

  // Verify AppUserModelID.
  base::win::ScopedPropVariant pv_appid;
  ASSERT_HRESULT_SUCCEEDED(
      pps->GetValue(PKEY_AppUserModel_ID, pv_appid.Receive()));
  EXPECT_EQ(pv_appid.get().vt, VT_LPWSTR);
  EXPECT_NE(std::wstring(pv_appid.get().pwszVal).find(L"app_search_in_chrome"),
            std::wstring::npos);

  // Verify RelaunchCommand.
  base::win::ScopedPropVariant pv_relaunch;
  ASSERT_HRESULT_SUCCEEDED(
      pps->GetValue(PKEY_AppUserModel_RelaunchCommand, pv_relaunch.Receive()));
  EXPECT_EQ(pv_relaunch.get().vt, VT_LPWSTR);
  std::wstring relaunch_command = pv_relaunch.get().pwszVal;
  EXPECT_NE(relaunch_command.find(L"chrome_proxy.exe"), std::wstring::npos);
  EXPECT_NE(relaunch_command.find(L"--omnibox-everywhere"), std::wstring::npos);

  // Verify RelaunchDisplayName matches the localized resource.
  base::win::ScopedPropVariant pv_name;
  ASSERT_HRESULT_SUCCEEDED(pps->GetValue(
      PKEY_AppUserModel_RelaunchDisplayNameResource, pv_name.Receive()));
  EXPECT_EQ(pv_name.get().vt, VT_LPWSTR);
  EXPECT_EQ(std::wstring(pv_name.get().pwszVal),
            base::UTF16ToWide(l10n_util::GetStringUTF16(
                IDS_SETTINGS_OMNIBOX_EVERYWHERE_TITLE)));

  widget->CloseNow();
}

}  // namespace omnibox_everywhere
