# Critical User Journey Action Catalog for the dPWA product

This file catalogs all of the actions that can be used to build critical user journeys for the dPWA product.

Existing documentation lives [here](/docs/webapps/integration-testing-framework.md).

TODO(dmurph): Move more documentation here. https://crbug.com/1314822

## How this file is parsed

The tables in this file are parsed as action templates for critical user journeys. Lines are considered an action template if:
- The first non-whitespace character is a `|`
- Splitting the line using the `|` character as a delimiter, the first item (stripping whitespace):
  - Does not start with `#`
  - Is not `---`
  - Is not empty


## Actions Table

TODO(dmurph): Possibly this table up into markdown-header section.

| # Action base name | Argument Types | Output Actions | Unique Identifier (next: 176) | Status (WIP, Implemented, Not Implemented, Parameterized) | Description | Metadata, implementation bug, etc |
| --- | --- | --- | --- | --- | --- | --- |
| # Badging |
| check_app_badge_empty | Site |  | 2 | Not Implemented | Check that the 'badge' on the app icon is empty |  |
| check_app_badge_has_value | Site |  | 3 | Not Implemented | Check that the 'badge' on the app icon has a value |  |
| clear_app_badge | Site |  | 4 | Not Implemented | The WebApp clears the 'badge' value from it's icon |  |
| set_app_badge | Site |  | 6 | Not Implemented | Set the app badge for the given site to a value. |  |
| |
| # Manifest Update |
| handle_app_identity_update_dialog_response | UpdateDialogResponse |  | 91 | Implemented | Click Accept or Uninstall in the App Identity Update dialog | finnur@ |
| manifest_update_scope_to | Site, Site |  | 8 | Implemented | Update the scope of the app at the first site to the second site. |  |
| manifest_update_icon | Site, UpdateDialogResponse |  | 68 | Implemented | Updates the launcher icon in the manifest of the website. | finnur@ |
| manifest_update_title | Site, Title, UpdateDialogResponse |  | 88 | Implemented | The website updates it's manifest.json to change the 'title' | finnur@ |
| manifest_update_colors | Site |  | 80 | Not Implemented | The website updates it's manifest.json to change the 'theme' color | P3 |
| manifest_update_display | Site, Display |  | 116 | Implemented |  |  |
| await_manifest_update | Site |  | 117 | WIP | Does any actions necessary (like closing browser windows) and blocks the execution of the test until the manifest has been updated for the given site. |  |
| |
| # Run on OS Login |
| apply_run_on_os_login_policy_allowed | Site |  | 100 | Implemented | Apply WebAppSettings policy for run_on_os_login to be allowed | phillis@ |
| apply_run_on_os_login_policy_blocked | Site |  | 101 | Implemented | Apply WebAppSettings policy for run_on_os_login to be blocked | phillis@ |
| apply_run_on_os_login_policy_run_windowed | Site |  | 102 | Implemented | Apply WebAppSettings policy for run_on_os_login to be run_windowed | phillis@ |
| disable_run_on_os_login_from_app_settings | Site |  | 153 | Implemented | Disable run on os login from app settings page | phillis@ |
| enable_run_on_os_login_from_app_settings | Site |  | 154 | Implemented | Enable run on os login from app settings page | phillis@ |
| enable_run_on_os_login | Site | enable_run_on_os_login_from_app_settings($1) & enable_run_on_os_login_from_app_home($1) | 155 | Parameterized | Enable an app to run on OS login from the app settings or app home page | dibyapal@ |
| disable_run_on_os_login | Site | disable_run_on_os_login_from_app_settings($1) & disable_run_on_os_login_from_app_home($1) | 156 | Parameterized | Disable an app from running on OS login from the app settings or app home page | dibyapal@ |
| remove_run_on_os_login_policy | Site |  | 103 | Implemented | Remove  run_on_os_login policy for the app in WebAppSettings policy. | phillis@ |
| |
| # Create Shortcut |
| create_shortcut | Site, WindowOptions |  | 29 | Implemented | Use the 'create shortcut' functionality at the given location using the "Create Shortcut" menu option (3-dot->"More Tools"->"Create Shortcut)  The WindowOption specified if the "Open in a window" checkbox should be selected. |  |
| |
| # Install |
| install_omnibox_icon | InstallableSite |  | 31 | Implemented |  |  |
| install_policy_app | Site, ShortcutOptions, WindowOptions, InstallMode |  | 32 | Implemented | Add a force-installed enterprise policy site to the user profile (must be managed profile). This installation action also opens the target site in a tab to match the expectation of installs opening the app first for some CUJs.|  |
| install_menu_option | InstallableSite |  | 47 | Implemented |  |  |
| install_no_shortcut | Site | install_policy_app($1, NoShortcut, WindowOptions::All, WebApp) | 56 | Parameterized |  |  |
| install_tabbed_no_shortcut | Site | install_policy_app($1, NoShortcut, Browser, WebApp) | 129 | Parameterized | All installation methods that result in a tabbed webapp without shortcut. |  |
| install_windowed_no_shortcut | Site | install_policy_app($1, NoShortcut, Windowed, WebApp) | 131 | Parameterized | All installation methods that result in a windowed webapp without shortcut. |  |
| install_by_user_windowed | InstallableSite | install_omnibox_icon($1) & install_menu_option($1) | 137 | Parameterized | All user installation methods that result in a windowed webapp without shortcut. |  |
| # Install & Create Shortcut Parameterized |
| install_or_shortcut | Site | create_shortcut($1, WindowOptions::All) & install_omnibox_icon($1) & install_policy_app($1, ShortcutOptions::All, WindowOptions::All, WebApp) & install_menu_option($1) | 52 | Parameterized |  |  |
| install_or_shortcut_by_user | Site | create_shortcut($1, WindowOptions::All) & install_omnibox_icon($1) & install_menu_option($1) | 53 | Parameterized |  |  |
| install_or_shortcut_by_user_tabbed | Site | create_shortcut($1, Browser) | 54 | Parameterized |  |  |
| install_or_shortcut_by_user_windowed | Site | create_shortcut($1, Windowed) & install_omnibox_icon($1) & install_menu_option($1) | 55 | Parameterized |  |  |
| install_or_shortcut_tabbed | Site | create_shortcut($1, Browser) & install_policy_app($1, ShortcutOptions::All, Browser, WebApp) | 61 | Parameterized | All installation methods that result in a tabbed webapp. |  |
| install_or_shortcut_tabbed_with_shortcut | Site | create_shortcut($1, Browser) & install_policy_app($1, WithShortcut, Browser) | 128 | Parameterized | All installation methods that result in a tabbed webapp with shortcut created. |  |
| install_or_shortcut_windowed | Site | create_shortcut($1, Windowed) & install_omnibox_icon($1) & install_policy_app($1, ShortcutOptions::All, Windowed, WebApp) & install_menu_option($1) | 62 | Parameterized | All installation methods that result in a windowed webapp. |  |
| install_or_shortcut_windowed_with_shortcut | Site | create_shortcut($1, Windowed) & install_omnibox_icon($1) &  install_policy_app($1, WithShortcut, Windowed, WebApp) & install_menu_option($1) | 130 | Parameterized | All installation methods that result in a windowed webapp with shortcut created. |  |
| install_or_shortcut_by_user_windowed_with_shortcut | Site | create_shortcut($1, Windowed) & install_omnibox_icon($1) & install_menu_option($1) | 136 | Parameterized | All user initiated installation methods that result in a windowed webapp with shortcut created. |  |
| install_or_shortcut_with_shortcut | Site | install_policy_app($1, WithShortcut, WindowOptions::All, WebApp) & create_shortcut($1, WindowOptions::All) & install_omnibox_icon($1) & install_menu_option($1) | 63 | Parameterized |  |  |
| |
| # Uninstall |
| uninstall_from_os | Site |  | 87 | Implemented | Uninstalls the app from OS integration - e.g. Windows Control Panel / Start menu |  |
| uninstall_from_app_settings | Site |  | 98 | Implemented | uninstall an app from app settings page, the app has to be locally installed. | phillis@ |
| uninstall_from_menu | Site |  | 43 | Implemented | Uninstall the webapp from the 3-dot menu in the webapp window |  |
| uninstall_policy_app | Site |  | 44 | Implemented | Remove a force-installed policy app to the user profile (must be managed profile) |  |
| uninstall_by_user | Site | uninstall_from_list($1) & uninstall_from_menu($1) & uninstall_from_os($1) & uninstall_from_app_settings($1) | 67 | Parameterized |  |  |
| uninstall_not_locally_installed | Site | uninstall_from_list($1) & uninstall_from_menu($1) & uninstall_from_os($1) | 99 | Parameterized | Uninstall an app by user, the app can be not locally installed. |  |
| # Checking app state |
| check_app_icon | Site, Color |  | 110 | Implemented | Check that the app icon color is correct | finnur@ |
| check_app_navigation | Site |  | 133 | Implemented |  |  |
| check_app_navigation_is_start_url |  |  | 14 | Implemented |  |  |
| check_theme_color | Site |  | 76 | Not Implemented | Asserts that the theme color of the given app window is correct. | P3 |
| # Misc UX |
| check_browser_navigation | Site |  | 134 | Implemented | Check the current browser navigation is the given site |  |
| check_browser_navigation_is_app_settings | Site |  | 109 | Implemented | Check the current browser navigation is chrome://app-settings/<app-id> | phillis@ |
| check_create_shortcut_not_shown |  |  | 85 | WIP | Check that the "Create Shortcut" menu option (3-dot->"More Tools"->"Create Shortcut) is greyed out |  |
| check_create_shortcut_shown |  |  | 86 | WIP | Check that the "Create Shortcut" menu option (3-dot->"More Tools"->"Create Shortcut) is shown |  |
| check_custom_toolbar |  |  | 16 | Implemented | Check that the PWA window has a custom toolbar to show the out-of-scope url. |  |
| check_platform_shortcut_not_exists | Site |  | 84 | WIP | The desktop platform shortcut has been removed. | cliffordcheng@, doc |
| check_install_icon_not_shown |  |  | 17 | Implemented | Check that the "Install" icon in the omnibox is not shown |  |
| check_install_icon_shown |  |  | 18 | Implemented | Check that the "Install" icon in the omnibox is shown |  |
| check_launch_icon_not_shown |  |  | 19 | Implemented |  |  |
| check_launch_icon_shown |  |  | 20 | Implemented |  |  |
| check_no_toolbar |  |  | 21 | Implemented |  |  |
| check_platform_shortcut_and_icon | Site |  | 7 | Implemented | The icon of the platform shortcut (on the desktop) is correct | cliffordcheng@, doc |
| check_run_on_os_login_disabled | Site |  | 107 | Implemented | Check run on os login is disabled. | phillis@ |
| check_run_on_os_login_enabled | Site |  | 106 | Implemented | Check run on os login is enabled. | phillis@ |
| check_tab_created | Number |  | 22 | Implemented | A given number of tabs were created in a chrome browser window |  |
| check_tab_not_created |  |  | 94 | Implemented | A tab was not created by the last state change action | cliffordcheng@, P1 |
| check_user_cannot_set_run_on_os_login | Site | check_user_cannot_set_run_on_os_login_app_settings($1) & check_user_cannot_set_run_on_os_login_app_home($1) | 159 | Parameterized | Check an user is unable to change the run on os login state from UI surfaces due to policy.  | dibyapal@ |
| check_user_cannot_set_run_on_os_login_app_settings | Site | | 158 | Implemented | Check user can't change the app's run_on_os_login state from the app settings page. | |
| check_window_closed |  |  | 23 | Implemented | The window was closed |  |
| check_window_created |  |  | 24 | Implemented | A window was created. |  |
| check_window_not_created |  |  | 127 | Implemented | A window was not created. | P2 |
| check_pwa_window_created | Site, Number |  | 123 | Implemented | A given number of windows were created for the given pwa. |  |
| check_pwa_window_created_in_profile | Site, Number, ProfileName |  | 165 | Implemented | A given number of windows were created for the given pwa in the given profile. |  |
| check_window_display_minimal |  |  | 25 | Implemented | Check that the window is a PWA window, and has minimal browser controls. |  |
| check_window_display_tabbed |  |  | 144 | Implemented | Check that the window is a PWA window, and has tabbed display mode. |  |
| check_window_display_standalone |  |  | 26 | Implemented | Check that the window is a PWA window, and has no browser controls. |  |
| close_custom_toolbar |  |  | 27 | Implemented | Press the 'x' button on the custom toolbar that is towards the top of the WebApp window. |  |
| close_pwa |  |  | 28 | Implemented | Close the WebApp window. |  |
| maybe_close_pwa |  |  | 143 | Implemented | Close the current app window if there is one open. |  |
| quit_app_shim | Site |  | 164 | Implemented | Closes the WebApp in all profiles by quitting the App Shim; Mac OS Only. |  |
| open_app_settings | Site | open_app_settings_from_chrome_apps($1) & open_app_settings_from_app_menu($1) & open_app_settings_from_command($1) | 95 | Parameterized | Launch chrome://app-settings/<app-id> page | phillis@ |
| open_app_settings_from_app_menu | Site |  | 97 | Implemented |  | phillis@ |
| open_app_settings_from_command | Site |  | 163 | Implemented | Open app settings via its browser command. |  |
| open_in_chrome |  |  | 71 | Implemented | Click on the 'open in chrome' link in the 3-dot menu of the app window | cliffordcheng@, P1 |
| set_open_in_tab | Site | set_open_in_tab_from_app_settings($1) & set_open_in_tab_from_app_home($1) | 148 | Parameterized | All methods to toggle an app to open in a tab in the same window. | dibyapal@ |
| set_open_in_tab_from_app_settings | Site |  | 149 | Implemented | Toggle the "open in window"  option in the chrome://app-settings/<app-id> page to disable an app from opening in a separate window, so that the app opens in a tab in the same window. | dibyapal@ |
| set_open_in_window | Site | set_open_in_window_from_app_settings($1) & set_open_in_window_from_app_home($1) | 146 | Parameterized | All methods to toggle an app to open in window. | dibyapal@ |
| set_open_in_window_from_app_settings | Site |  | 150 | Implemented | Toggle the "open in window" option in the chrome://app-settings/<app-id> page to enable an app to launch in a separate window. | dibyapal@ |
| check_window_color_correct | Site |  | 77 | Not Implemented | The color of the window is correct. | P3 |
| check_window_icon_correct | Site |  | 78 | Not Implemented |  | P3 |
| delete_platform_shortcut | Site |  | 74 | Implemented | Delete the shortcut that lives on the operating system. Win/Mac/Linux only. | P2 |
| delete_profile |  |  | 83 | Not Implemented | Delete the user profile. | P4 |
| enter_full_screen_app |  |  | 168 | Implemented | Enter full screen mode for the app window. | P1 |
| exit_full_screen_app |  |  | 169 | Implemented | Exit full screen mode for the app window. | P1 |
| check_window_controls_overlay_toggle_icon | IsShown |  | 170 | Implemented | Checks if the Window Controls Overlay icon exists. | P1 |
| # Launching |
| launch_from_launch_icon | Site |  | 35 | Implemented | Launch the web app by navigating the browser to the web app, and selecting the launch icon in the omnibox (intent picker), |  |
| launch_from_menu_option | Site |  | 69 | Implemented | Launch the web app by navigating the browser to the web app, and selecting the "Launch _" menu option in the 3-dot menu. | cliffordcheng@, P1 |
| launch_from_platform_shortcut | Site |  | 1 | Implemented | Launch an app from a platform shortcut on the user's desktop or start menu. | cliffordcheng@, P0 |
| launch | Site | launch_from_menu_option($1) & launch_from_launch_icon($1) & launch_from_chrome_apps($1) & launch_from_platform_shortcut($1) | 64 | Parameterized |  |  |
| launch_not_from_platform_shortcut | Site | launch_from_menu_option($1) & launch_from_launch_icon($1) & launch_from_chrome_apps($1) | 135 | Parameterized |  |  |
| launch_from_browser | Site | launch_from_menu_option($1) & launch_from_launch_icon($1) & launch_from_chrome_apps($1) | 65 | Parameterized |  |  |
| check_app_loaded_in_tab | Site |  | 163 | Implemented | Verify that the web app was launched in a tab after being clicked from chrome://apps. |  |
| # Navigation |
| navigate_browser | Site |  | 37 | Implemented | Navigate the browser to one of the static sites provided by the testing framework. |  |
| navigate_notfound_url |  |  | 38 | Implemented | Navigate to a url that returns a 404 server error. |  |
| navigate_pwa | Site, Site |  | 39 | Implemented | Navigates the PWA window of the first site pwa to the second site. |  |
| navigate_crashed_url |  |  | 81 | Not Implemented | Navigate to a page that crashes, or simulates a crashed renderer. chrome://crash | P3 |
| navigate_link_target_blank |  |  | 82 | Not Implemented | Click on a href link on the current page, where the target of the link is "_blank" | P3 |
| # Cross-device syncing |
| switch_profile_clients | ProfileClient |  | 40 | Implemented | Switch to a different instance of chrome signed in to the same profile |  |
| sync_turn_off |  |  | 41 | Implemented | Turn chrome sync off for "Apps": chrome://settings/syncSetup/advanced in all profiles |  |
| sync_turn_on |  |  | 42 | Implemented | Turn chrome sync on for "Apps": chrome://settings/syncSetup/advanced in all profiles |  |
| sync_sign_out |  |  | 174 | Implemented | Sign out of chrome sync in the current profile |  |
| sync_sign_in |  |  | 175 | Implemented | Sign in to chrome sync in the current profile |  |
| switch_incognito_profile |  |  | 73 | Implemented | Switch to using incognito mode | P2 |
| switch_active_profile | ProfileName |  | 160 | Implemented | Switch to using a different profile |  |
| # File handling |
| check_site_handles_file | Site, FileExtension |  | 118 | Implemented |  |  |
| check_site_not_handles_file | Site, FileExtension |  | 122 | Implemented |  |  |
| check_file_handling_dialog | IsShown |  | 119 | Not Implemented |  |  |
| launch_file_expect_dialog | Site, FilesOptions, AllowDenyOptions, AskAgainOptions |  | 120 | Implemented |  |  |
| launch_file_expect_no_dialog | Site, FilesOptions |  | 121 | Implemented |  |  |
| check_files_loaded_in_site | Site, FilesOptions |  | 126 | Not Implemented | Check that the appropriate file contents have loaded in in PWA windows. |  |
| add_file_handling_policy_approval | Site |  | 124 | Not Implemented |  |  |
| remove_file_handling_policy_approval | Site |  | 125 | Not Implemented |  |  |
| enable_file_handling | Site |  | 161 | Implemented |  |  |
| disable_file_handling | Site |  | 162 | Implemented |  |  |
| # Window Controls Overlay
| check_window_controls_overlay_toggle | Site, IsShown |  | 112 | WIP |  |  |
| check_window_controls_overlay | Site, IsOn |  | 113 | WIP |  |  |
| enable_window_controls_overlay | Site |  | 114 | WIP |  |  |
| disable_window_controls_overlay | Site |  | 115 | WIP |  |  |
| #Subapps |
| install_sub_app | Site, Site, SubAppInstallDialogOptions |  | 138 | WIP | Navigate to the first site, call subApps.add() to install the second site. |  |
| remove_sub_app | Site, Site |  | 139 | Implemented | Navigate to the first site, call subApps.remove() to uninstall the second site. |  |
| check_has_sub_app | Site, Site |  | 140 | Implemented | Assuming we have some tab or browser window on the (potential) parent site, call subApps.list() and check if the given site is listed. |  |
| check_not_has_sub_app | Site, Site |  | 141 | Implemented | Assuming we have some tab or browser window on the (potential) parent site, call subApps.list() and check if the given site is not listed. |  |
| check_no_sub_apps | Site |  | 142 | Implemented | Assuming we have some tab or browser window on the (potential) parent site, call subApps.list() and check if the list is empty. |  |
| # Tabbed |
| new_app_tab | Site | | 171 | Implemented | Opens a new tab in the given app. | |
| check_app_tab_is_site | Site, Number | | 172 | Implemented | Checks the tab at the given index of the web app is the given site. | |
| check_app_tab_created | | | 173 | Implemented | Checks a tab was added to the web app window. | |

### App Home
Actions that the user can take by going to chrome://apps and either left clicking an app or right clicking an app and then taking actions from the context menu that opens.

| # Action base name | Argument Types | Output Actions | Unique Identifier | Status (WIP, Implemented, Not Implemented, Parameterized) | Description | Metadata, implementation bug, etc |
| --- | --- | --- | --- | --- | --- | --- |
| set_open_in_window_from_app_home | Site |  | 145 | Implemented | Checks the `open in window` checkbox from the chrome://apps context menu by right clicking an app icon. | dibyapal@ |
| set_open_in_tab_from_app_home | Site |  | 147 | Implemented | Unchecks the `open in window` checkbox from the chrome://apps context menu by right clicking an app icon. | dibyapal@ |
| check_app_in_list_not_locally_installed | Site |  | 45 | Implemented | Find the app in the app list (chrome://apps) and check that the given app is in the app list and is not installed. This means the icon is grey, and right clicking on it provides an 'install' option. Win/Mac/Linux only. |  |
| check_app_not_in_list | Site |  | 15 | Implemented | Check that the given app is NOT in the app list. On desktop, this is chrome://apps, and on ChromeOS, this is the app drawer. |  |
| check_app_title | Site, Title |  | 79 | Implemented | Check that the app title is correct | finnur@ |
| check_app_in_list_icon_correct | Site |  | 75 | Implemented | Find the app in the app list (on desktop, this is chrome://apps, and on ChromeOS, this is the app drawer). Check that the icon for the given app in the app list is correct. | P2 (fetch icon using web request for chrome://app-icon/<app-id>/<icon-size>) |
| check_app_in_list_tabbed | Site |  | 11 | Implemented | Find the app in the app list (on desktop, this is chrome://apps, and on ChromeOS, this is the app drawer). Check that the app opens in a window by right clicking on it to see if the "open in window" option is checked, and by launching it to see if it opens in a separate window. |  |
| check_app_in_list_windowed | Site |  | 12 | Implemented | Find the app in the app list (on desktop, this is chrome://apps, and on ChromeOS, this is the app drawer). Check that the app opens in a tab by right clicking on it to see if the "open in window" option is unchecked, and by launching it to see if it opens in a browser tab (and not a window). |  |
| check_app_list_empty |  |  | 13 | Implemented | The app list is empty (on desktop, this is chrome://apps, and on ChromeOS, this is the app drawer). |  |
| enable_run_on_os_login_from_app_home | Site |  | 151 | Implemented | Checks the `launch at startup` checkbox from the chrome://apps context menu by right clicking an app icon. | dibyapal@ |
| disable_run_on_os_login_from_app_home | Site |  | 152 | Implemented | Unchecks the `launch at startup` checkbox from the chrome://apps context menu by right clicking an app icon. | dibyapal@ |
| check_user_cannot_set_run_on_os_login_app_home | Site | | 157 | Implemented | Checks that the user cannot set the `launch at startup` checkbox from the chrome://apps context menu by right clicking an app icon. | dibyapal@ |
| install_locally | Site |  | 46 | Implemented | Find the app in the app list (chrome://apps) and install it by right-clicking on the app and selecting the 'install' option. Win/Mac/Linux only. |  |
| uninstall_from_list | Site |  | 10 | Implemented | Uninstall the webapp from wherever apps are listed by chrome. On WML, this is from chrome://apps, and on ChromeOS, this is from the 'launcher' |  |
| create_shortcuts_from_list | Site |  | 72 | Implemented | "create shortcuts" in chrome://apps. Win/Mac/Linux only. | P2 |
| open_app_settings_from_chrome_apps | Site |  | 96 | Implemented |  | phillis@ |
| launch_from_chrome_apps | Site |  | 34 | Implemented | Launch the web app by navigating to chrome://apps, and then clicking on the app icon. |  |
| navigate_app_home |  |  | 166 | Implemented | Navigate to chrome://apps in the current browser. |  |
| check_browser_not_at_app_home |  |  | 167 | Implemented | Check the current browser is not at chrome://apps. |  |
