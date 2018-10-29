// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_UTILS_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_UTILS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "components/arc/common/app.mojom.h"
#include "components/arc/metrics/arc_metrics_constants.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace arc {

extern const char kGoogleDuo[];
extern const char kInfinitePainter[];
extern const char kLightRoom[];
extern const char kPlayStoreAppId[];
extern const char kPlayBooksAppId[];
extern const char kPlayGamesAppId[];
extern const char kPlayMoviesAppId[];
extern const char kPlayMusicAppId[];
extern const char kPlayStorePackage[];
extern const char kPlayStoreActivity[];
extern const char kSettingsAppId[];
extern const char kInitialStartParam[];
extern const char kSettingsAppDomainUrlActivity[];

// Represents unparsed intent.
class Intent {
 public:
  Intent();
  ~Intent();

  enum LaunchFlags : uint32_t {
    FLAG_ACTIVITY_NEW_TASK = 0x10000000,
    FLAG_RECEIVER_NO_ABORT = 0x08000000,
    FLAG_ACTIVITY_RESET_TASK_IF_NEEDED = 0x00200000,
    FLAG_ACTIVITY_LAUNCH_ADJACENT = 0x00001000,
  };

  void AddExtraParam(const std::string& extra_param);
  bool HasExtraParam(const std::string& extra_param) const;

  const std::string& action() const { return action_; }
  void set_action(const std::string& action) { action_ = action; }

  const std::string& category() const { return category_; }
  void set_category(const std::string& category) { category_ = category; }

  const std::string& package_name() const { return package_name_; }
  void set_package_name(const std::string& package_name) {
    package_name_ = package_name;
  }

  const std::string& activity() const { return activity_; }
  void set_activity(const std::string& activity) { activity_ = activity; }

  uint32_t launch_flags() const { return launch_flags_; }
  void set_launch_flags(uint32_t launch_flags) { launch_flags_ = launch_flags; }

  const std::vector<std::string>& extra_params() { return extra_params_; }

 private:
  std::string action_;                     // Extracted from action.
  std::string category_;                   // Extracted from category.
  std::string package_name_;               // Extracted from component.
  std::string activity_;                   // Extracted from component.
  uint32_t launch_flags_ = 0;              // Extracted from launchFlags;
  std::vector<std::string> extra_params_;  // Other parameters not listed above.

  DISALLOW_COPY_AND_ASSIGN(Intent);
};

// Checks if a given app should be hidden in launcher.
bool ShouldShowInLauncher(const std::string& app_id);

// Launch Android Settings app.
bool LaunchAndroidSettingsApp(content::BrowserContext* context,
                              int event_flags,
                              int64_t display_id);

// Launch Play Store app.
bool LaunchPlayStoreWithUrl(const std::string& url);

// Launches an ARC app.
bool LaunchApp(content::BrowserContext* context,
               const std::string& app_id,
               int event_flags,
               UserInteractionType user_action);
bool LaunchApp(content::BrowserContext* context,
               const std::string& app_id,
               int event_flags,
               UserInteractionType user_action,
               int64_t display_id);

bool LaunchAppWithIntent(content::BrowserContext* context,
                         const std::string& app_id,
                         const base::Optional<std::string>& launch_intent,
                         int event_flags,
                         UserInteractionType user_action,
                         int64_t display_id);

// Launches App Shortcut that was published by Android's ShortcutManager.
bool LaunchAppShortcutItem(content::BrowserContext* context,
                           const std::string& app_id,
                           const std::string& shortcut_id,
                           int64_t display_id);

// Launches a specific activity within Settings app on ARC.
bool LaunchSettingsAppActivity(content::BrowserContext* context,
                               const std::string& activity,
                               int event_flags,
                               int64_t display_id);

// Sets task active.
void SetTaskActive(int task_id);

// Closes the task.
void CloseTask(int task_id);

// Opens TalkBack settings window.
void ShowTalkBackSettings();

// Starts Play Auto Install flow.
void StartPaiFlow();

// Gets user selected package names.
std::vector<std::string> GetSelectedPackagesFromPrefs(
    content::BrowserContext* context);

// Starts Play Fast App Reinstall flow.
void StartFastAppReinstallFlow(const std::vector<std::string>& package_names);

// Uninstalls the package in ARC.
void UninstallPackage(const std::string& package_name);

// Uninstalls ARC app or removes shortcut.
void UninstallArcApp(const std::string& app_id, Profile* profile);

// Removes cached app shortcut icon in ARC.
void RemoveCachedIcon(const std::string& icon_resource_id);

// Shows package info for ARC package at the specified page.
bool ShowPackageInfo(const std::string& package_name,
                     mojom::ShowPackageInfoPage page,
                     int64_t display_id);

// Returns true if |id| represents either ARC app or ARC shelf group.
bool IsArcItem(content::BrowserContext* context, const std::string& id);

// Returns intent that can be used to launch an activity specified by
// |package_name| and |activity|. |extra_params| is the list of optional
// parameters encoded to intent.
std::string GetLaunchIntent(const std::string& package_name,
                            const std::string& activity,
                            const std::vector<std::string>& extra_params);

// Parses provided |intent_as_string|. Returns false if |intent_as_string|
// cannot be parsed.
bool ParseIntent(const std::string& intent_as_string, Intent* intent);

// Returns current active locale and list of preferred languages for the given
// |profile|.
void GetLocaleAndPreferredLanguages(const Profile* profle,
                                    std::string* out_locale,
                                    std::string* out_preferred_languages);

}  // namespace arc

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_UTILS_H_
