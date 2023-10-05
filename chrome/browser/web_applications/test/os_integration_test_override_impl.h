// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_OS_INTEGRATION_TEST_OVERRIDE_IMPL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_OS_INTEGRATION_TEST_OVERRIDE_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"

#if BUILDFLAG(IS_WIN)
#include "base/containers/flat_map.h"
#include "base/test/test_reg_util_win.h"
#endif

class Profile;

#if BUILDFLAG(IS_WIN)
class ShellLinkItem;
#endif

namespace web_app {

#if BUILDFLAG(IS_LINUX)
struct LinuxFileRegistration {
  base::FilePath file_name;
  std::string xdg_command;
  std::string file_contents;
};
#endif

// See the `OsIntegrationTestOverride` base class documentation for more
// information about the purpose of this class. This is the implementation, and
// being test-only can include test-only code.
//
// Other than inheriting the base class & providing implementations of those
// getters & setters, this class is also responsible for providing common ways
// of checking the OS integration state in a test. The class methods are
// organized per-os-integration.
class OsIntegrationTestOverrideImpl : public OsIntegrationTestOverride {
 public:
  using AppProtocolList =
      std::vector<std::tuple<webapps::AppId, std::vector<std::string>>>;
#if BUILDFLAG(IS_WIN)
  using JumpListEntryMap =
      base::flat_map<std::wstring, std::vector<scoped_refptr<ShellLinkItem>>>;
#endif
  // Destroying this class blocks the thread until all users of
  // OsIntegrationTestOverride::Get() have destroyed any saved
  // `scoped_refptr<OsIntegrationTestOverride>`.
  struct BlockingRegistration {
    BlockingRegistration();
    ~BlockingRegistration();

    scoped_refptr<OsIntegrationTestOverrideImpl> test_override;
  };

  // Returns the current test override. This will CHECK-fail if one does not
  // exist.
  static scoped_refptr<OsIntegrationTestOverrideImpl> Get();

  // Overrides applicable directories for shortcut integration and returns an
  // object that:
  // 1) Contains the directories.
  // 2) Keeps the override active until the object is destroyed.
  // 3) DCHECK-fails on destruction if any of the shortcut directories / os
  //    hooks are NOT cleanup by the test. This ensures that trybots don't have
  //    old test artifacts on them that can make future tests flaky.
  // All installs that occur during the lifetime of the
  // OsIntegrationTestOverrideImpl MUST be uninstalled before it is
  // destroyed.
  // The returned value, on destruction, will block until all usages of the
  // OsIntegrationTestOverride::Get() are destroyed.
  static std::unique_ptr<BlockingRegistration> OverrideForTesting(
      const base::FilePath& base_path = base::FilePath());

  // -------------------------------
  // === Simulating user actions ===
  // -------------------------------
  // These methods simulate users doing the given action on the operating
  // system.

  // Delete shortcuts stored in the test override for a specific app. This
  // should only be run on Windows, Mac and Linux.
  bool SimulateDeleteShortcutsByUser(Profile* profile,
                                     const webapps::AppId& app_id,
                                     const std::string& app_name);

#if BUILDFLAG(IS_MAC)
  bool DeleteChromeAppsDir();
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
  bool DeleteDesktopDirOnWin();
  bool DeleteApplicationMenuDirOnWin();
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX)
  bool DeleteDesktopDirOnLinux();
#endif  // BUILDFLAG(IS_LINUX)

  // -------------------------------
  // === Run on OS Login ===
  // -------------------------------

  // Looks into shortcuts stored for OS integration and returns if run on OS
  // login mode is enabled based on the location. This should only be run on
  // Windows, Mac and Linux.
  bool IsRunOnOsLoginEnabled(Profile* profile,
                             const webapps::AppId& app_id,
                             const std::string& app_name);

  // -------------------------------
  // === File Handling ===
  // -------------------------------

  bool IsFileExtensionHandled(Profile* profile,
                              const webapps::AppId& app_id,
                              std::string app_name,
                              std::string file_extension);

  // -------------------------------
  // === Shortcuts ===
  // -------------------------------

  // Reads the icon color for a specific shortcut that has been created.
  // For Mac and Win, the shortcut_dir field is mandatory. For all other OSes,
  // this can be an empty base::FilePath().
  // For ChromeOS and Linux, the size_px field is mandatory to prevent erroneous
  // results. For all other OSes, the field can be skipped. The default value of
  // size_px is usually filled up with kLauncherIconSize (see
  // chrome/browser/web_applications/web_app_icon_generator.h for more
  // information), which is 128.
  absl::optional<SkColor> GetShortcutIconTopLeftColor(
      Profile* profile,
      base::FilePath shortcut_dir,
      const webapps::AppId& app_id,
      const std::string& app_name,
      SquareSizePx size_px = icon_size::k128);

  // Gets the current shortcut path based on a shortcut directory, app_id and
  // app_name. This should only be run on Windows, Mac and Linux.
  base::FilePath GetShortcutPath(Profile* profile,
                                 base::FilePath shortcut_dir,
                                 const webapps::AppId& app_id,
                                 const std::string& app_name);

  // Looks into the current shortcut paths to determine if a shortcut has
  // been created or not. This should only be run on Windows, Mac and Linux.
  // TODO(crbug.com/1425967): Add PList parsing logic for Mac shortcut checking.
  bool IsShortcutCreated(Profile* profile,
                         const webapps::AppId& app_id,
                         const std::string& app_name);

  // ---------------------------------
  // === Shortcut menu / jump list ===
  // ---------------------------------

  bool AreShortcutsMenuRegistered();

#if BUILDFLAG(IS_WIN)
  std::vector<SkColor> GetIconColorsForShortcutsMenu(
      const std::wstring& app_user_model_id);
  int GetCountOfShortcutIconsCreated(const std::wstring& app_user_model_id);
  bool IsShortcutsMenuRegisteredForApp(const std::wstring& app_user_model_id);
#endif  // BUILDFLAG(IS_WIN)

  // ------------------------------
  // === Uninstall Registration ===
  // ------------------------------

#if BUILDFLAG(IS_WIN)
  // Returns true if the given app_id/name/profile is registered with the OS in
  // the uninstall menu, and false if it isn't. The unexpected value is a string
  // description of the error.
  base::expected<bool, std::string> IsUninstallRegisteredWithOs(
      const webapps::AppId& app_id,
      const std::string& app_name,
      Profile* profile);
#endif  // BUILDFLAG(IS_WIN)

  // -------------------------
  // === Protocol Handlers ===
  // -------------------------

  const AppProtocolList& protocol_scheme_registrations();

  // ---------------------------------
  // === OsIntegrationTestOverride ===
  // ---------------------------------

  OsIntegrationTestOverrideImpl* AsOsIntegrationTestOverrideImpl() override;

#if BUILDFLAG(IS_WIN)
  // These should not be called from tests, these are automatically
  // called from production code in testing to set
  // up OS integration data for shortcuts menu registration and
  // unregistration.
  void AddShortcutsMenuJumpListEntryForApp(
      const std::wstring& app_user_model_id,
      const std::vector<scoped_refptr<ShellLinkItem>>& shell_link_items)
      override;
  void DeleteShortcutsMenuJumpListEntryForApp(
      const std::wstring& app_user_model_id) override;
#endif

#if BUILDFLAG(IS_WIN)
  const base::FilePath& desktop() override;
  const base::FilePath& application_menu() override;
  const base::FilePath& quick_launch() override;
  const base::FilePath& startup() override;
#elif BUILDFLAG(IS_MAC)
  bool IsChromeAppsValid() override;
  const base::FilePath& chrome_apps_folder() override;
  void EnableOrDisablePathOnLogin(const base::FilePath& file_path,
                                  bool enable_on_login) override;
#elif BUILDFLAG(IS_LINUX)
  const base::FilePath& desktop() override;
  const base::FilePath& startup() override;
  const base::FilePath& applications_dir() override;
#endif

  // Creates a tuple of app_id to protocols and adds it to the vector
  // of registered protocols. There can be multiple entries for the same
  // app_id.
  void RegisterProtocolSchemes(const webapps::AppId& app_id,
                               std::vector<std::string> protocols) override;

 private:
  friend class base::RefCountedThreadSafe<OsIntegrationTestOverrideImpl>;

  explicit OsIntegrationTestOverrideImpl(const base::FilePath& base_path);
  ~OsIntegrationTestOverrideImpl() override;

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // Reads an icon file (.ico/.png/.icns) and returns the color at the
  // top left position (0,0).
  SkColor GetIconTopLeftColorFromShortcutFile(
      const base::FilePath& shortcut_path);
#endif

#if BUILDFLAG(IS_WIN)
  SkColor ReadColorFromShortcutMenuIcoFile(const base::FilePath& file_path);
#endif

#if BUILDFLAG(IS_WIN)
  base::ScopedTempDir desktop_;
  base::ScopedTempDir application_menu_;
  base::ScopedTempDir quick_launch_;
  base::ScopedTempDir startup_;

  // This is used to ensure any registry changes by this test don't affect other
  // parts of the the trybot and are cleaned up.
  registry_util::RegistryOverrideManager registry_override_;

  // Records all ShellLinkItems for a given AppUserModelId for handling
  // shortcuts menu registration.
  JumpListEntryMap jump_list_entry_map_;

#elif BUILDFLAG(IS_MAC)
  base::ScopedTempDir chrome_apps_folder_;
  std::map<base::FilePath, bool> startup_enabled_;

#elif BUILDFLAG(IS_LINUX)
  base::ScopedTempDir desktop_;
  base::ScopedTempDir startup_;
  base::ScopedTempDir applications_dir_;
  std::vector<LinuxFileRegistration> linux_file_registration_;
#endif

  // Records all registration events for a given app id & protocol list. Due to
  // simplification on the OS-side, unregistrations are not recorded, and
  // instead this list can be checked for an empty registration.
  AppProtocolList protocol_scheme_registrations_;

  base::flat_set<std::wstring> shortcut_menu_apps_registered_;

  // `on_destruction_` has it's closure set only once (when BlockingRegistration
  // is destroyed) and executed when OsIntegrationTestOverrideImpl is destroyed.
  // The destructor of BlockingRegistration
  // - Gets the lock to prevent multi-thread issues.
  // - Sets `on_destruction_` using a run loop.
  // - Destroys this object
  // - When waits on the closure to ensure destruction has completed everywhere.
  base::Lock destruction_closure_lock;
  base::ScopedClosureRunner on_destruction_
      GUARDED_BY(destruction_closure_lock);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_OS_INTEGRATION_TEST_OVERRIDE_IMPL_H_
