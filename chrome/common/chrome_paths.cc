// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/path_utils.h"
#include "base/base_paths_android.h"
// ui/base must only be used on Android. See BUILD.gn for dependency info.
#include "ui/base/ui_base_paths.h"  // nogncheck
#endif

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_OPENBSD)
#include "components/policy/core/common/policy_paths.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/registry.h"
#endif

#if BUILDFLAG(ENABLE_WIDEVINE)
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck
#endif

namespace {

std::optional<bool> g_override_using_default_data_directory_for_testing;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// The path to the external extension <id>.json files.
// /usr/share seems like a good choice, see: http://www.pathname.com/fhs/
const base::FilePath::CharType kFilepathSinglePrefExtensions[] =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    FILE_PATH_LITERAL("/usr/share/google-chrome/extensions");
#else
    FILE_PATH_LITERAL("/usr/share/chromium/extensions");
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_WIDEVINE)
// The name of the hint file that tells the latest component updated Widevine
// CDM directory. This file name should not be changed as otherwise existing
// Widevine CDMs might not be loaded.
const base::FilePath::CharType kComponentUpdatedWidevineCdmHint[] =
    FILE_PATH_LITERAL("latest-component-updated-widevine-cdm");
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

#if BUILDFLAG(IS_CHROMEOS)
const base::FilePath::CharType kDeviceRefreshTokenFilePath[] =
    FILE_PATH_LITERAL("/home/chronos/device_refresh_token");

bool GetChromeOsCrdDataDirInternal(base::FilePath* result,
                                   bool* should_be_created) {
#if BUILDFLAG(IS_CHROMEOS_DEVICE)
  *result = base::FilePath::FromASCII("/run/crd");
  // The directory is created by ChromeOS (since we do not have the permissions
  // to create anything in /run).
  *should_be_created = false;
  return true;
#else
  // On glinux-ChromeOS builds `/run/` doesn't exist, so we simply use the temp
  // directory.
  base::FilePath temp_directory;
  if (!base::PathService::Get(base::DIR_TEMP, &temp_directory)) {
    return false;
  }

  *result = temp_directory.Append(FILE_PATH_LITERAL("crd"));
  *should_be_created = true;
  return true;
#endif  // BUILDFLAG(IS_CHROMEOS_DEVICE)
}
#endif  // BUILDFLAG(IS_CHROMEOS)

base::FilePath& GetInvalidSpecifiedUserDataDirInternal() {
  static base::NoDestructor<base::FilePath> s;
  return *s;
}

// Gets the path for bundled implementations of components. Note that these
// implementations should not be used if higher-versioned component-updated
// implementations are available in DIR_USER_DATA.
bool GetComponentDirectory(base::FilePath* result) {
#if BUILDFLAG(IS_MAC)
  // If called from Chrome, return the framework's Libraries directory.
  if (base::apple::AmIBundled()) {
    *result = chrome::GetFrameworkBundlePath();
    DCHECK(!result->empty());
    *result = result->Append("Libraries");
    return true;
  }
// In tests, just look in the assets directory (below).
#endif

  // The rest of the world expects components in the assets directory.
  return base::PathService::Get(base::DIR_ASSETS, result);
}

}  // namespace

namespace chrome {

bool PathProvider(int key, base::FilePath* result) {
  // Some keys are just aliases...
  switch (key) {
    case chrome::DIR_LOGS:
#ifdef NDEBUG
      // Release builds write to the data dir
      return base::PathService::Get(chrome::DIR_USER_DATA, result);
#else
      // Debug builds write next to the binary (in the build tree)
#if BUILDFLAG(IS_MAC)
      // Apps may not write into their own bundle.
      if (base::apple::AmIBundled()) {
        return base::PathService::Get(chrome::DIR_USER_DATA, result);
      }
#endif  // BUILDFLAG(IS_MAC)
      return base::PathService::Get(base::DIR_EXE, result);
#endif  // NDEBUG
  }

  // Assume that we will not need to create the directory if it does not exist.
  // This flag can be set to true for the cases where we want to create it.
  bool create_dir = false;

  base::FilePath cur;
  switch (key) {
    case chrome::DIR_USER_DATA:
      if (!GetDefaultUserDataDirectory(&cur)) {
        return false;
      }
      create_dir = true;
      break;
    case chrome::DIR_USER_DOCUMENTS:
      if (!GetUserDocumentsDirectory(&cur)) {
        return false;
      }
      create_dir = true;
      break;
    case chrome::DIR_USER_MUSIC:
      if (!GetUserMusicDirectory(&cur)) {
        return false;
      }
      break;
    case chrome::DIR_USER_PICTURES:
      if (!GetUserPicturesDirectory(&cur)) {
        return false;
      }
      break;
    case chrome::DIR_USER_VIDEOS:
      if (!GetUserVideosDirectory(&cur)) {
        return false;
      }
      break;
    case chrome::DIR_DEFAULT_DOWNLOADS_SAFE:
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
      if (!GetUserDownloadsDirectorySafe(&cur)) {
        return false;
      }
      break;
#else
      // Fall through for all other platforms.
#endif
    case chrome::DIR_DEFAULT_DOWNLOADS:
#if BUILDFLAG(IS_ANDROID)
      if (!base::android::GetDownloadsDirectory(&cur)) {
        return false;
      }
#else
      if (!GetUserDownloadsDirectory(&cur)) {
        return false;
      }
      // Do not create the download directory here, we have done it twice now
      // and annoyed a lot of users.
#endif
      break;
    case chrome::DIR_CRASH_METRICS:
      if (!base::PathService::Get(chrome::DIR_USER_DATA, &cur)) {
        return false;
      }
      break;
    case chrome::DIR_CRASH_DUMPS:
// Only use /var/log/chrome on IS_CHROMEOS_DEVICE builds. For non-device
// ChromeOS builds we fall back to the #else below and store relative to the
// default user-data directory.
#if BUILDFLAG(IS_CHROMEOS_DEVICE)
      // ChromeOS uses a separate directory. See http://crosbug.com/25089
      cur = base::FilePath("/var/log/chrome");
#elif BUILDFLAG(IS_ANDROID)
      if (!base::android::GetCacheDirectory(&cur)) {
        return false;
      }
#else
      // The crash reports are always stored relative to the default user data
      // directory.  This avoids the problem of having to re-initialize the
      // exception handler after parsing command line options, which may
      // override the location of the app's profile directory.
      // TODO(scottmg): Consider supporting --user-data-dir. See
      // https://crbug.com/565446.
      if (!GetDefaultUserDataDirectory(&cur)) {
        return false;
      }
#endif
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
      cur = cur.Append(FILE_PATH_LITERAL("Crashpad"));
#else
      cur = cur.Append(FILE_PATH_LITERAL("Crash Reports"));
#endif
      create_dir = true;
      break;
    case chrome::DIR_LOCAL_TRACES:
#if BUILDFLAG(IS_ANDROID)
      if (!base::PathService::Get(base::DIR_CACHE, &cur)) {
        return false;
      }
#else
      if (!base::PathService::Get(chrome::DIR_USER_DATA, &cur)) {
        return false;
      }
#endif
      cur = cur.Append(FILE_PATH_LITERAL("Local Traces"));
      create_dir = true;
      break;
#if BUILDFLAG(IS_WIN)
    case chrome::DIR_WATCHER_DATA:
      // The watcher data is always stored relative to the default user data
      // directory.  This allows the watcher to be initialized before
      // command-line options have been parsed.
      if (!GetDefaultUserDataDirectory(&cur)) {
        return false;
      }
      cur = cur.Append(FILE_PATH_LITERAL("Diagnostics"));
      break;
    case chrome::DIR_ROAMING_USER_DATA:
      if (!GetDefaultRoamingUserDataDirectory(&cur)) {
        return false;
      }
      create_dir = true;
      break;
#endif
    case chrome::DIR_RESOURCES:
#if BUILDFLAG(IS_MAC)
      cur = base::apple::FrameworkBundlePath();
      cur = cur.Append(FILE_PATH_LITERAL("Resources"));
#else
      if (!base::PathService::Get(base::DIR_ASSETS, &cur)) {
        return false;
      }
      cur = cur.Append(FILE_PATH_LITERAL("resources"));
#endif
      break;
    case chrome::DIR_APP_DICTIONARIES:
#if !BUILDFLAG(IS_WIN)
      // On most platforms, we can't write into the directory where
      // binaries are stored, so keep dictionaries in the user data dir.
      if (!base::PathService::Get(chrome::DIR_USER_DATA, &cur)) {
        return false;
      }
#else
      // TODO(crbug.com/40840089): Migrate Windows to use `DIR_USER_DATA` like
      // other platforms.
      if (!base::PathService::Get(base::DIR_EXE, &cur)) {
        return false;
      }
#endif
      cur = cur.Append(FILE_PATH_LITERAL("Dictionaries"));
      create_dir = true;
      break;
    case chrome::DIR_COMPONENTS:
      if (!GetComponentDirectory(&cur)) {
        return false;
      }
      break;
    case chrome::FILE_LOCAL_STATE:
      if (!base::PathService::Get(chrome::DIR_USER_DATA, &cur)) {
        return false;
      }
      cur = cur.Append(chrome::kLocalStateFilename);
      break;
    case chrome::FILE_RECORDED_SCRIPT:
      if (!base::PathService::Get(chrome::DIR_USER_DATA, &cur)) {
        return false;
      }
      cur = cur.Append(FILE_PATH_LITERAL("script.log"));
      break;

#if BUILDFLAG(ENABLE_WIDEVINE)
    case chrome::DIR_BUNDLED_WIDEVINE_CDM:
      if (!GetComponentDirectory(&cur)) {
        return false;
      }
      cur = cur.AppendASCII(kWidevineCdmBaseDirectory);
      break;

    case chrome::DIR_COMPONENT_UPDATED_WIDEVINE_CDM: {
      if (!base::PathService::Get(chrome::DIR_USER_DATA, &cur)) {
        return false;
      }
      cur = cur.AppendASCII(kWidevineCdmBaseDirectory);
      break;
    }
    case chrome::FILE_COMPONENT_WIDEVINE_CDM_HINT:
      if (!base::PathService::Get(chrome::DIR_COMPONENT_UPDATED_WIDEVINE_CDM,
                                  &cur)) {
        return false;
      }
      cur = cur.Append(kComponentUpdatedWidevineCdmHint);
      break;
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

    case chrome::FILE_RESOURCES_PACK:  // Falls through.
    case chrome::FILE_DEV_UI_RESOURCES_PACK:
#if BUILDFLAG(IS_MAC)
      cur = base::apple::FrameworkBundlePath();
      cur = cur.Append(FILE_PATH_LITERAL("Resources"))
                .Append(FILE_PATH_LITERAL("resources.pak"));
#elif BUILDFLAG(IS_ANDROID)
      if (!base::PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &cur)) {
        return false;
      }
      if (key == chrome::FILE_DEV_UI_RESOURCES_PACK) {
        cur = cur.Append(FILE_PATH_LITERAL("dev_ui_resources.pak"));
      } else {
        DCHECK_EQ(chrome::FILE_RESOURCES_PACK, key);
        cur = cur.Append(FILE_PATH_LITERAL("resources.pak"));
      }
#else
      // If we're not bundled on mac or Android, resources.pak should be in
      // the "assets" location (e.g. next to the binary, on many platforms).
      if (!base::PathService::Get(base::DIR_ASSETS, &cur)) {
        return false;
      }
      cur = cur.Append(FILE_PATH_LITERAL("resources.pak"));
#endif
      break;

#if BUILDFLAG(IS_CHROMEOS)
    case chrome::DIR_CHROMEOS_CRD_DATA:
      if (!GetChromeOsCrdDataDirInternal(&cur,
                                         /*should_be_created=*/&create_dir)) {
        return false;
      }
      break;
#endif

    // The following are only valid in the development environment, and
    // will fail if executed from an installed executable (because the
    // generated path won't exist).
    case chrome::DIR_GEN_TEST_DATA:
      if (!base::PathService::Get(base::DIR_OUT_TEST_DATA_ROOT, &cur)) {
        return false;
      }
      cur = cur.Append(FILE_PATH_LITERAL("test_data"));
      if (!base::PathExists(cur)) {  // We don't want to create this.
        return false;
      }
      break;
    case chrome::DIR_TEST_DATA:
      if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &cur)) {
        return false;
      }
      cur = cur.Append(FILE_PATH_LITERAL("chrome"));
      cur = cur.Append(FILE_PATH_LITERAL("test"));
      cur = cur.Append(FILE_PATH_LITERAL("data"));
      if (!base::PathExists(cur)) {  // We don't want to create this.
        return false;
      }
      break;
    case chrome::DIR_TEST_TOOLS:
      if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &cur)) {
        return false;
      }
      cur = cur.Append(FILE_PATH_LITERAL("chrome"));
      cur = cur.Append(FILE_PATH_LITERAL("tools"));
      cur = cur.Append(FILE_PATH_LITERAL("test"));
      if (!base::PathExists(cur)) {  // We don't want to create this
        return false;
      }
      break;
#if BUILDFLAG(IS_MAC)
    case chrome::DIR_OUTER_BUNDLE: {
      cur = base::apple::OuterBundlePath();
      break;
    }
#endif
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_OPENBSD)
    case chrome::DIR_POLICY_FILES: {
      cur = base::FilePath(policy::kPolicyPath);
      break;
    }
#endif
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) && BUILDFLAG(CHROMIUM_BRANDING))
    case chrome::DIR_USER_EXTERNAL_EXTENSIONS: {
      if (!base::PathService::Get(chrome::DIR_USER_DATA, &cur)) {
        return false;
      }
      cur = cur.Append(FILE_PATH_LITERAL("External Extensions"));
      break;
    }
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    case chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS: {
      cur = base::FilePath(kFilepathSinglePrefExtensions);
      break;
    }
#endif
    case chrome::DIR_EXTERNAL_EXTENSIONS:
#if BUILDFLAG(IS_MAC)
      if (!chrome::GetGlobalApplicationSupportDirectory(&cur)) {
        return false;
      }

      cur = cur.Append(FILE_PATH_LITERAL("Google"))
                .Append(FILE_PATH_LITERAL("Chrome"))
                .Append(FILE_PATH_LITERAL("External Extensions"));
#else
      if (!base::PathService::Get(base::DIR_MODULE, &cur)) {
        return false;
      }

      cur = cur.Append(FILE_PATH_LITERAL("extensions"));
      create_dir = true;
#endif
      break;

    case chrome::DIR_DEFAULT_APPS:
#if BUILDFLAG(IS_MAC)
      cur = base::apple::FrameworkBundlePath();
      cur = cur.Append(FILE_PATH_LITERAL("Default Apps"));
#else
      if (!base::PathService::Get(base::DIR_MODULE, &cur)) {
        return false;
      }
      cur = cur.Append(FILE_PATH_LITERAL("default_apps"));
#endif
      break;

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE) &&                                   \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
     BUILDFLAG(IS_ANDROID))
    case chrome::DIR_NATIVE_MESSAGING:
#if BUILDFLAG(IS_MAC)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      cur = base::FilePath(
          FILE_PATH_LITERAL("/Library/Google/Chrome/NativeMessagingHosts"));
#else
      cur = base::FilePath(FILE_PATH_LITERAL(
          "/Library/Application Support/Chromium/NativeMessagingHosts"));
#endif
#else  // BUILDFLAG(IS_MAC)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      cur = base::FilePath(
          FILE_PATH_LITERAL("/etc/opt/chrome/native-messaging-hosts"));
#else
      cur = base::FilePath(
          FILE_PATH_LITERAL("/etc/chromium/native-messaging-hosts"));
#endif
#endif  // !BUILDFLAG(IS_MAC)
      break;

    case chrome::DIR_USER_NATIVE_MESSAGING:
      if (!base::PathService::Get(chrome::DIR_USER_DATA, &cur)) {
        return false;
      }
      cur = cur.Append(FILE_PATH_LITERAL("NativeMessagingHosts"));
      break;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE) && (BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID))
#if !BUILDFLAG(IS_ANDROID)
    case chrome::DIR_GLOBAL_GCM_STORE:
      if (!base::PathService::Get(chrome::DIR_USER_DATA, &cur)) {
        return false;
      }
      cur = cur.Append(kGCMStoreDirname);
      break;
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
    case chrome::FILE_CHROME_OS_DEVICE_REFRESH_TOKEN:
      cur = base::FilePath(kDeviceRefreshTokenFilePath);
      break;
#endif  // BUILDFLAG(IS_CHROMEOS)

    case chrome::DIR_OPTIMIZATION_GUIDE_PREDICTION_MODELS:
      if (!base::PathService::Get(chrome::DIR_USER_DATA, &cur)) {
        return false;
      }
      cur = cur.Append(FILE_PATH_LITERAL("OptimizationGuidePredictionModels"));
      create_dir = true;
      break;

    default:
      return false;
  }

  // TODO(bauerb): http://crbug.com/259796
  base::ScopedAllowBlocking allow_blocking;
  if (create_dir && !base::PathExists(cur) && !base::CreateDirectory(cur)) {
    return false;
  }

  *result = cur;
  return true;
}

std::optional<bool> IsUsingDefaultDataDirectory() {
  if (g_override_using_default_data_directory_for_testing.has_value()) {
    return g_override_using_default_data_directory_for_testing.value();
  }

  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    return std::nullopt;
  }

  base::FilePath default_user_data_dir;
  if (!chrome::GetDefaultUserDataDirectory(&default_user_data_dir)) {
    return std::nullopt;
  }

  return user_data_dir == default_user_data_dir;
}

void SetUsingDefaultUserDataDirectoryForTesting(
    std::optional<bool> is_default) {
  g_override_using_default_data_directory_for_testing = is_default;
}

// This cannot be done as a static initializer sadly since Visual Studio will
// eliminate this object file if there is no direct entry point into it.
void RegisterPathProvider() {
  base::PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

void SetInvalidSpecifiedUserDataDir(const base::FilePath& user_data_dir) {
  GetInvalidSpecifiedUserDataDirInternal() = user_data_dir;
}

const base::FilePath& GetInvalidSpecifiedUserDataDir() {
  return GetInvalidSpecifiedUserDataDirInternal();
}

}  // namespace chrome
