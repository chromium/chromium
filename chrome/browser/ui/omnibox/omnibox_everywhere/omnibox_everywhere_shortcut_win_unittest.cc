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

#include <set>
#include <string>

#include "base/base_paths_win.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
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
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_icon_resources_win.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"
#include "ui/views/win/hwnd_util.h"

namespace omnibox_everywhere {
namespace {

// Path at which the Start Menu shortcut is expected inside `start_menu_dir`.
base::FilePath ShortcutPathIn(const base::FilePath& start_menu_dir) {
  return start_menu_dir.Append(base::StrCat({GetDisplayName(), L".lnk"}));
}

// Returns a COM STA sequence matching the one the controller uses in
// production.
base::SequenceBound<OmniboxEverywhereShortcutHelperWin> MakeBoundHelper() {
  return base::SequenceBound<OmniboxEverywhereShortcutHelperWin>(
      base::ThreadPool::CreateCOMSTATaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));
}

}  // namespace

class OmniboxEverywhereShortcutWinTest : public ChromeViewsTestBase {
 public:
  OmniboxEverywhereShortcutWinTest() = default;
  ~OmniboxEverywhereShortcutWinTest() override = default;
};

TEST_F(OmniboxEverywhereShortcutWinTest, GetAppUserModelId) {
  std::wstring app_id = GetAppUserModelId();
  EXPECT_FALSE(app_id.empty());
  EXPECT_NE(app_id.find(L"app_search_with_chrome"), std::wstring::npos);
}

TEST_F(OmniboxEverywhereShortcutWinTest, GetDisplayNameIsChannelSpecific) {
  std::set<std::wstring> names;

  for (int index = 0; index < install_static::NUM_INSTALL_MODES; ++index) {
    install_static::ScopedInstallDetails install_details(
        /*system_level=*/false, index);
    const std::wstring name = GetDisplayName();
    // An empty name would reduce the Start Menu entry to a bare ".lnk".
    EXPECT_FALSE(name.empty());
    names.insert(name);
  }

  // A mode missing its own string falls back to the primary one and collides.
  EXPECT_EQ(names.size(),
            static_cast<size_t>(install_static::NUM_INSTALL_MODES));
}

TEST_F(OmniboxEverywhereShortcutWinTest, GetDisplayNameUsesPrimaryModeString) {
  install_static::ScopedInstallDetails install_details(
      /*system_level=*/false, /*install_mode_index=*/0);
  EXPECT_EQ(GetDisplayName(), base::UTF16ToWide(l10n_util::GetStringUTF16(
                                  IDS_OMNIBOX_EVERYWHERE_NAME)));
}

TEST_F(OmniboxEverywhereShortcutWinTest, CreateStartMenuShortcutPerChannel) {
  base::ScopedTempDir start_menu_dir;
  ASSERT_TRUE(start_menu_dir.CreateUniqueTempDir());
  base::ScopedPathOverride start_menu_override(base::DIR_START_MENU,
                                               start_menu_dir.GetPath());

  for (int index = 0; index < install_static::NUM_INSTALL_MODES; ++index) {
    install_static::ScopedInstallDetails install_details(
        /*system_level=*/false, index);
    OmniboxEverywhereShortcutHelperWin helper;
    EXPECT_TRUE(helper.CreateStartMenuShortcut());
    EXPECT_TRUE(base::PathExists(ShortcutPathIn(start_menu_dir.GetPath())));
  }

  // Every install mode must leave behind its own Start Menu entry rather than
  // overwriting the previous one.
  base::FileEnumerator shortcuts(start_menu_dir.GetPath(), /*recursive=*/false,
                                 base::FileEnumerator::FILES,
                                 FILE_PATH_LITERAL("*.lnk"));
  int shortcut_count = 0;
  for (base::FilePath path = shortcuts.Next(); !path.empty();
       path = shortcuts.Next()) {
    ++shortcut_count;
  }
  EXPECT_EQ(shortcut_count, install_static::NUM_INSTALL_MODES);
}

TEST_F(OmniboxEverywhereShortcutWinTest, CreateStartMenuShortcut) {
  base::ScopedTempDir start_menu_dir;
  ASSERT_TRUE(start_menu_dir.CreateUniqueTempDir());
  base::ScopedPathOverride start_menu_override(base::DIR_START_MENU,
                                               start_menu_dir.GetPath());

  OmniboxEverywhereShortcutHelperWin helper;
  EXPECT_TRUE(helper.CreateStartMenuShortcut());

  base::FilePath shortcut_path = ShortcutPathIn(start_menu_dir.GetPath());
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
  EXPECT_NE(properties.icon.value().find(chrome::kBrowserProcessExecutableName),
            std::wstring::npos);
  EXPECT_EQ(properties.icon_index,
            static_cast<int>(icon_resources::kOmniboxEverywhereIndex));
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

  base::SequenceBound<OmniboxEverywhereShortcutHelperWin> helper =
      MakeBoundHelper();

  base::test::TestFuture<bool> future;
  helper.AsyncCall(&OmniboxEverywhereShortcutHelperWin::CreateStartMenuShortcut)
      .Then(future.GetCallback());
  EXPECT_TRUE(future.Get());

  base::FilePath shortcut_path = ShortcutPathIn(start_menu_dir.GetPath());
  EXPECT_TRUE(base::PathExists(shortcut_path));
}

TEST_F(OmniboxEverywhereShortcutWinTest, SetWindowPropertiesNullHwndSafe) {
  // Verifies that calling SetWindowProperties with a null HWND
  // does not crash in either ephemeral or persistent mode.
  SetWindowProperties(nullptr, /*is_ephemeral=*/true, /*allow_pinning=*/true);
  SetWindowProperties(nullptr, /*is_ephemeral=*/false, /*allow_pinning=*/true);
  SetWindowProperties(nullptr, /*is_ephemeral=*/false, /*allow_pinning=*/false);
}

TEST_F(OmniboxEverywhereShortcutWinTest, SetWindowPropertiesEphemeralMode) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                       views::Widget::InitParams::TYPE_WINDOW);
  HWND hwnd = views::HWNDForWidget(widget.get());
  ASSERT_NE(hwnd, nullptr);

  // Ephemeral windows are hidden from the taskbar, so pinning is suppressed
  // regardless of whether a Start Menu shortcut is available.
  SetWindowProperties(hwnd, /*is_ephemeral=*/true, /*allow_pinning=*/true);

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

TEST_F(OmniboxEverywhereShortcutWinTest,
       CreateStartMenuShortcutCreatesMissing) {
  base::ScopedTempDir start_menu_dir;
  ASSERT_TRUE(start_menu_dir.CreateUniqueTempDir());
  base::ScopedPathOverride start_menu_override(base::DIR_START_MENU,
                                               start_menu_dir.GetPath());

  base::FilePath shortcut_path = ShortcutPathIn(start_menu_dir.GetPath());
  ASSERT_FALSE(base::PathExists(shortcut_path));

  base::SequenceBound<OmniboxEverywhereShortcutHelperWin> helper =
      MakeBoundHelper();
  base::test::TestFuture<bool> future;
  helper.AsyncCall(&OmniboxEverywhereShortcutHelperWin::CreateStartMenuShortcut)
      .Then(future.GetCallback());
  EXPECT_TRUE(future.Get());
  EXPECT_TRUE(base::PathExists(shortcut_path));
}

TEST_F(OmniboxEverywhereShortcutWinTest, CreateStartMenuShortcutKeepsExisting) {
  base::ScopedTempDir start_menu_dir;
  ASSERT_TRUE(start_menu_dir.CreateUniqueTempDir());
  base::ScopedPathOverride start_menu_override(base::DIR_START_MENU,
                                               start_menu_dir.GetPath());

  OmniboxEverywhereShortcutHelperWin helper;
  ASSERT_TRUE(helper.CreateStartMenuShortcut());

  // Tags the up-to-date shortcut so that an unwanted rewrite is detectable;
  // the description is not one of the properties compared against.
  const base::FilePath shortcut_path = ShortcutPathIn(start_menu_dir.GetPath());
  base::win::ShortcutProperties tag;
  tag.set_description(L"sentinel");
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      shortcut_path, tag, base::win::ShortcutOperation::kUpdateExisting));

  EXPECT_TRUE(helper.CreateStartMenuShortcut());

  base::win::ShortcutProperties properties;
  ASSERT_TRUE(base::win::ResolveShortcutProperties(
      shortcut_path, base::win::ShortcutProperties::PROPERTIES_DESCRIPTION,
      &properties));
  EXPECT_EQ(properties.description, L"sentinel");
}

TEST_F(OmniboxEverywhereShortcutWinTest, CreateStartMenuShortcutUpdatesStale) {
  base::ScopedTempDir start_menu_dir;
  ASSERT_TRUE(start_menu_dir.CreateUniqueTempDir());
  base::ScopedPathOverride start_menu_override(base::DIR_START_MENU,
                                               start_menu_dir.GetPath());

  // Stands in for a shortcut left behind by an earlier install.
  const base::FilePath shortcut_path = ShortcutPathIn(start_menu_dir.GetPath());
  base::win::ShortcutProperties stale;
  stale.set_target(
      start_menu_dir.GetPath().Append(FILE_PATH_LITERAL("stale_target.exe")));
  stale.set_arguments(L"--stale-switch");
  stale.set_app_id(L"Stale.AppUserModelId");
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      shortcut_path, stale, base::win::ShortcutOperation::kCreateAlways));

  OmniboxEverywhereShortcutHelperWin helper;
  EXPECT_TRUE(helper.CreateStartMenuShortcut());

  base::win::ShortcutProperties properties;
  ASSERT_TRUE(base::win::ResolveShortcutProperties(
      shortcut_path,
      base::win::ShortcutProperties::PROPERTIES_TARGET |
          base::win::ShortcutProperties::PROPERTIES_ARGUMENTS |
          base::win::ShortcutProperties::PROPERTIES_APP_ID,
      &properties));
  EXPECT_NE(
      properties.target.value().find(FILE_PATH_LITERAL("chrome_proxy.exe")),
      std::wstring::npos);
  EXPECT_NE(properties.arguments.find(L"--omnibox-everywhere"),
            std::wstring::npos);
  EXPECT_EQ(properties.app_id, GetAppUserModelId());
}

TEST_F(OmniboxEverywhereShortcutWinTest,
       CreateStartMenuShortcutReplacesUnreadable) {
  base::ScopedTempDir start_menu_dir;
  ASSERT_TRUE(start_menu_dir.CreateUniqueTempDir());
  base::ScopedPathOverride start_menu_override(base::DIR_START_MENU,
                                               start_menu_dir.GetPath());

  // A file that does not parse as a shortcut must not be mistaken for one.
  const base::FilePath shortcut_path = ShortcutPathIn(start_menu_dir.GetPath());
  ASSERT_TRUE(base::WriteFile(shortcut_path, "not a shortcut"));

  OmniboxEverywhereShortcutHelperWin helper;
  EXPECT_TRUE(helper.CreateStartMenuShortcut());

  base::win::ShortcutProperties properties;
  ASSERT_TRUE(base::win::ResolveShortcutProperties(
      shortcut_path, base::win::ShortcutProperties::PROPERTIES_APP_ID,
      &properties));
  EXPECT_EQ(properties.app_id, GetAppUserModelId());
}

TEST_F(OmniboxEverywhereShortcutWinTest,
       CreateStartMenuShortcutReportsFailure) {
  // Point DIR_START_MENU at a regular file so the shortcut cannot be created.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath not_a_directory =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("not_a_directory"));
  ASSERT_TRUE(base::WriteFile(not_a_directory, "content"));
  base::ScopedPathOverride start_menu_override(base::DIR_START_MENU,
                                               not_a_directory,
                                               /*is_absolute=*/true,
                                               /*create=*/false);

  OmniboxEverywhereShortcutHelperWin helper;
  EXPECT_FALSE(helper.CreateStartMenuShortcut());
}

TEST_F(OmniboxEverywhereShortcutWinTest, SetWindowPropertiesPersistentMode) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                       views::Widget::InitParams::TYPE_WINDOW);
  HWND hwnd = views::HWNDForWidget(widget.get());
  ASSERT_NE(hwnd, nullptr);

  SetWindowProperties(hwnd, /*is_ephemeral=*/false, /*allow_pinning=*/true);

  Microsoft::WRL::ComPtr<IPropertyStore> pps;
  ASSERT_HRESULT_SUCCEEDED(
      SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&pps)));

  // Persistent windows stay pinnable when `allow_pinning` is true.
  base::win::ScopedPropVariant pv_prevent;
  ASSERT_HRESULT_SUCCEEDED(
      pps->GetValue(PKEY_AppUserModel_PreventPinning, pv_prevent.Receive()));
  EXPECT_EQ(pv_prevent.get().vt, VT_EMPTY);

  // Verify AppUserModelID.
  base::win::ScopedPropVariant pv_appid;
  ASSERT_HRESULT_SUCCEEDED(
      pps->GetValue(PKEY_AppUserModel_ID, pv_appid.Receive()));
  EXPECT_EQ(pv_appid.get().vt, VT_LPWSTR);
  EXPECT_NE(
      std::wstring(pv_appid.get().pwszVal).find(L"app_search_with_chrome"),
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
  EXPECT_EQ(std::wstring(pv_name.get().pwszVal), GetDisplayName());

  widget->CloseNow();
}

TEST_F(OmniboxEverywhereShortcutWinTest,
       SetWindowPropertiesPersistentModeWithoutPinning) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                       views::Widget::InitParams::TYPE_WINDOW);
  HWND hwnd = views::HWNDForWidget(widget.get());
  ASSERT_NE(hwnd, nullptr);

  SetWindowProperties(hwnd, /*is_ephemeral=*/false, /*allow_pinning=*/false);

  Microsoft::WRL::ComPtr<IPropertyStore> pps;
  ASSERT_HRESULT_SUCCEEDED(
      SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&pps)));

  base::win::ScopedPropVariant pv_prevent;
  ASSERT_HRESULT_SUCCEEDED(
      pps->GetValue(PKEY_AppUserModel_PreventPinning, pv_prevent.Receive()));
  EXPECT_EQ(pv_prevent.get().vt, VT_BOOL);
  EXPECT_EQ(pv_prevent.get().boolVal, VARIANT_TRUE);

  // Suppressing pinning must not cost the window its own taskbar grouping and
  // icon, which the AppUserModelId provides.
  base::win::ScopedPropVariant pv_appid;
  ASSERT_HRESULT_SUCCEEDED(
      pps->GetValue(PKEY_AppUserModel_ID, pv_appid.Receive()));
  EXPECT_EQ(pv_appid.get().vt, VT_LPWSTR);
  EXPECT_NE(
      std::wstring(pv_appid.get().pwszVal).find(L"app_search_with_chrome"),
      std::wstring::npos);

  widget->CloseNow();
}

}  // namespace omnibox_everywhere
