# Critical User Journeys for the dPWA Product

This file lists all critical user journeys that are required to have full test coverage of the dPWA product.

Existing documentation lives [here](/docs/webapps/integration-testing-framework.md).

TODO(dmurph): Move more documentation here. https://crbug.com/1314822

[[TOC]]

## How this file is parsed

The tables are parsed in this file as critical user journeys. Lines are considered a CUJ if:
- The first non-whitespace character is a `|`
- Splitting the line using the `|` character as a delimiter, the first item (stripping whitespace):
  - Does not start with `#`
  - Is not `---`
  - Is not empty

## App Identity Updating tests
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_by_user_windowed(Standalone) |  launch(Standalone) | check_app_title(Standalone, StandaloneOriginal) |
| WMLC | install_by_user_windowed(Standalone) | manifest_update_title(Standalone, StandaloneUpdated, AcceptUpdate) | await_manifest_update | launch(Standalone) | check_app_title(Standalone, StandaloneUpdated) |
| WMLC | install_by_user_windowed(Standalone) | manifest_update_title(Standalone, StandaloneUpdated, CancelUninstallAndAcceptUpdate) | await_manifest_update | launch(Standalone) | check_app_title(Standalone, StandaloneUpdated) |
| WMLC | install_by_user_windowed(Standalone) | manifest_update_title(Standalone, StandaloneUpdated, CancelDialogAndUninstall) | await_manifest_update | check_app_not_in_list | check_platform_shortcut_not_exists |
| WMLC | install_by_user_windowed(Standalone) | manifest_update_icon(Standalone, AcceptUpdate) | await_manifest_update | check_app_icon(Standalone, Red) |
| WMLC | install_policy_app(Standalone, ShortcutOptions::All, Windowed, WebApp) | manifest_update_title(Standalone, StandaloneUpdated, SkipDialog) | await_manifest_update | launch_from_platform_shortcut(Standalone) | check_app_title(Standalone, StandaloneUpdated) |

## Run on OS Login
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WML | install_or_shortcut | apply_run_on_os_login_policy_blocked | check_user_cannot_set_run_on_os_login |
| WML | install_or_shortcut | enable_run_on_os_login | apply_run_on_os_login_policy_blocked | check_run_on_os_login_disabled |
| WML | install_or_shortcut | apply_run_on_os_login_policy_run_windowed | check_run_on_os_login_enabled |
| WML | install_or_shortcut | apply_run_on_os_login_policy_run_windowed | check_user_cannot_set_run_on_os_login |
| WML | install_or_shortcut | enable_run_on_os_login | check_run_on_os_login_enabled |
| WML | install_or_shortcut | enable_run_on_os_login | disable_run_on_os_login | check_run_on_os_login_disabled |
| WML | install_or_shortcut | apply_run_on_os_login_policy_run_windowed | remove_run_on_os_login_policy | check_run_on_os_login_disabled |
| WML | install_or_shortcut | enable_run_on_os_login | apply_run_on_os_login_policy_blocked | remove_run_on_os_login_policy | check_run_on_os_login_enabled |
| WML | install_or_shortcut_by_user_windowed | switch_profile_clients(Client2) | switch_profile_clients(Client1) | sync_turn_off | uninstall_by_user | switch_profile_clients(Client2) | check_app_not_in_list |
| WML | install_or_shortcut_by_user_windowed | switch_profile_clients(Client2) | switch_profile_clients(Client2) |  apply_run_on_os_login_policy_run_windowed | check_run_on_os_login_disabled |
| WML | install_or_shortcut_by_user_windowed | switch_profile_clients(Client2) | switch_profile_clients(Client2) |  apply_run_on_os_login_policy_run_windowed | install_locally | check_run_on_os_login_enabled |
| WML | install_policy_app(Standalone, NoShortcut, Windowed, WebApp) | apply_run_on_os_login_policy_allowed | disable_run_on_os_login | check_run_on_os_login_disabled |
| WML | install_policy_app(Standalone, NoShortcut, Windowed, WebApp) | apply_run_on_os_login_policy_allowed | enable_run_on_os_login | check_run_on_os_login_enabled |

## Badging
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_or_shortcut_by_user_windowed | set_app_badge | clear_app_badge | check_app_badge_empty |
| WMLC | install_or_shortcut_by_user_windowed | set_app_badge | check_app_badge_has_value |
| WMLC | navigate_browser(Standalone) | set_app_badge | check_platform_shortcut_not_exists |
| # Toolbar |
| WMLC | install_or_shortcut_windowed | navigate_pwa(Standalone, MinimalUi) | close_custom_toolbar | check_app_navigation_is_start_url |
| WMLC | install_or_shortcut_by_user_windowed | navigate_pwa(Standalone, MinimalUi) | check_custom_toolbar |
| # Initial state sanity checks |
| WMLC | navigate_browser(Standalone) | check_app_not_in_list |
| WMLC | navigate_browser(Standalone) | check_platform_shortcut_not_exists |
| WMLC | navigate_browser(NotPromotable) | check_app_not_in_list |
| WMLC | navigate_browser(NotPromotable) | check_platform_shortcut_not_exists |

# Installation
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_or_shortcut(Standalone) | check_app_title(Standalone, StandaloneOriginal) |
| WMLC | install_omnibox_icon(Screenshots) |
| WMLC | install_or_shortcut_by_user_windowed | check_window_created |
| WMLC | install_no_shortcut | check_platform_shortcut_not_exists |
| WMLC | install_no_shortcut(NotPromotable) | check_platform_shortcut_not_exists(NotPromotable) |
| WMLC | install_or_shortcut_tabbed | check_app_in_list_tabbed |
| WMLC | install_or_shortcut_tabbed | navigate_browser(Standalone) | check_create_shortcut_shown |
| WMLC | install_or_shortcut_tabbed | navigate_browser(Standalone) | check_install_icon_shown |
| WMLC | install_or_shortcut_tabbed | navigate_browser(Standalone) | check_launch_icon_not_shown |
| WMLC | install_or_shortcut_tabbed(NotPromotable) | check_app_in_list_tabbed(NotPromotable) |
| WMLC | install_or_shortcut_tabbed(NotPromotable) | navigate_browser(NotPromotable) | check_create_shortcut_shown |
| WMLC | install_or_shortcut_tabbed(NotPromotable) | navigate_browser(NotPromotable) | check_install_icon_not_shown |
| WMLC | install_or_shortcut_tabbed(NotPromotable) | navigate_browser(NotPromotable) | check_launch_icon_not_shown |
| WMLC | install_or_shortcut_windowed | check_app_in_list_windowed |
| WMLC | install_or_shortcut_windowed | navigate_browser(Standalone) | check_create_shortcut_not_shown |
| WMLC | install_or_shortcut_windowed | navigate_browser(Standalone) | check_install_icon_not_shown |
| WMLC | install_or_shortcut_windowed | navigate_browser(Standalone) | check_launch_icon_shown |
| WMLC | install_or_shortcut_windowed(MinimalUi) | navigate_browser(MinimalUi) | check_launch_icon_shown |
| WMLC | install_or_shortcut_windowed(Tabbed) | check_app_in_list_windowed(Tabbed) |
| WMLC | install_or_shortcut_windowed(Tabbed) | navigate_browser(Tabbed) | check_launch_icon_shown | check_install_icon_not_shown |
| WMLC | install_or_shortcut_windowed(NotPromotable) | check_app_in_list_windowed(NotPromotable) |
| WMLC | install_or_shortcut_windowed(NotPromotable) | navigate_browser(NotPromotable) | check_create_shortcut_not_shown |
| WMLC | install_or_shortcut_windowed(NotPromotable) | navigate_browser(NotPromotable) | check_install_icon_not_shown |
| WMLC | install_or_shortcut_windowed(NotPromotable) | navigate_browser(NotPromotable) | check_launch_icon_shown |
| WMLC | install_or_shortcut_with_shortcut | check_platform_shortcut_and_icon |
| WMLC | install_or_shortcut_with_shortcut(NotPromotable) | check_platform_shortcut_and_icon(NotPromotable) |
| WMLC | install_or_shortcut_by_user_tabbed(Standalone) | launch_from_platform_shortcut(Standalone) | check_tab_created(One) |
| WMLC | install_or_shortcut_by_user_tabbed(Standalone) | launch_from_platform_shortcut(Standalone) | install_omnibox_icon(Standalone) | check_pwa_window_created_in_profile(Standalone, One, Default)
| WMLC | create_shortcut(Standalone, Windowed) | check_window_created |
| WMLC | create_shortcut(Standalone, Windowed) | close_pwa | check_app_in_list_windowed(Standalone) | check_platform_shortcut_and_icon(Standalone) |
| WMLC | create_shortcut(Standalone, Windowed) | close_pwa | launch_from_platform_shortcut(Standalone) | check_pwa_window_created_in_profile(Standalone, One, Default) | check_launch_icon_not_shown |
| WMLC | install_menu_option(ChromeUrl) | check_window_created |
| WMLC | create_shortcut(ChromeUrl, Windowed) | check_window_created |

## Uninstallation
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WML | install_or_shortcut_by_user_windowed | uninstall_by_user | check_app_not_in_list |
| WML | install_or_shortcut_by_user_windowed | uninstall_by_user | navigate_browser(Standalone) | check_install_icon_shown |
| WML | install_or_shortcut_by_user_windowed | uninstall_by_user | navigate_browser(Standalone) | check_launch_icon_not_shown |
| WML | install_or_shortcut_by_user_windowed | uninstall_by_user | check_platform_shortcut_not_exists |
| WML | install_or_shortcut_by_user_windowed(NotPromotable) | uninstall_by_user(NotPromotable) | check_app_not_in_list |
| WML | install_or_shortcut_by_user_windowed(NotPromotable) | uninstall_by_user(NotPromotable) | check_platform_shortcut_not_exists(NotPromotable) |
| WMLC | install_or_shortcut_by_user_tabbed | uninstall_from_list | check_app_not_in_list |
| WMLC | install_or_shortcut_by_user_tabbed | uninstall_from_list | navigate_browser(Standalone) | check_install_icon_shown |
| WMLC | install_or_shortcut_by_user_tabbed | uninstall_from_list | navigate_browser(Standalone) | check_launch_icon_not_shown |
| WMLC | install_or_shortcut_by_user_tabbed | uninstall_from_list | check_platform_shortcut_not_exists |
| C | install_or_shortcut_by_user_windowed | uninstall_from_list | check_app_not_in_list |
| C | install_or_shortcut_by_user_windowed | uninstall_from_list | navigate_browser(Standalone) | check_install_icon_shown |
| C | install_or_shortcut_by_user_windowed | uninstall_from_list | navigate_browser(Standalone) | check_launch_icon_not_shown |
| C | install_or_shortcut_by_user_windowed | uninstall_from_list | check_platform_shortcut_not_exists |
| WMLC | install_or_shortcut_by_user_tabbed(NotPromotable) | uninstall_from_list(NotPromotable) | check_app_not_in_list |
| WMLC | install_or_shortcut_by_user_tabbed(NotPromotable) | uninstall_from_list(NotPromotable) | check_platform_shortcut_not_exists(NotPromotable) |
| C | install_or_shortcut_by_user_windowed(NotPromotable) | uninstall_from_list(NotPromotable) | check_app_not_in_list |
| C | install_or_shortcut_by_user_windowed(NotPromotable) | uninstall_from_list(NotPromotable) | check_platform_shortcut_not_exists(NotPromotable) |
| WMLC | install_policy_app(Standalone, ShortcutOptions::All, WindowOptions::All, WebApp) | uninstall_policy_app | check_app_not_in_list |
| WMLC | install_policy_app(Standalone, ShortcutOptions::All, Browser, WebApp) | uninstall_policy_app | navigate_browser(Standalone) | check_install_icon_shown |
| WMLC | install_policy_app(Standalone, ShortcutOptions::All, Browser, WebApp) | uninstall_policy_app | navigate_browser(Standalone) | check_launch_icon_not_shown |
| WMLC | install_policy_app(Standalone, ShortcutOptions::All, WindowOptions::All, WebApp) | uninstall_policy_app | check_platform_shortcut_not_exists | check_app_not_in_list |

# Launch behavior tests
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_or_shortcut_windowed | launch_not_from_platform_shortcut | check_window_created |
| WLC | install_or_shortcut_windowed | launch_from_platform_shortcut | check_window_created |
| M | install_or_shortcut_by_user_windowed | launch_from_platform_shortcut | check_window_not_created |
| M | install_policy_app(Standalone, ShortcutOptions::All, Windowed, WebApp) | launch_from_platform_shortcut | check_window_created |
| WMLC | install_or_shortcut_windowed | launch | check_window_display_standalone |
| WMLC | install_or_shortcut_tabbed | set_open_in_window | launch | check_window_created |
| WML | install_or_shortcut_windowed | set_open_in_tab | launch_from_chrome_apps | check_tab_not_created | check_app_loaded_in_tab |
| C | install_or_shortcut_windowed | set_open_in_tab | launch_from_chrome_apps | check_tab_created(One) | check_app_loaded_in_tab |
| WLC | install_or_shortcut_windowed | set_open_in_tab | launch_from_platform_shortcut | check_tab_created(One) | check_app_loaded_in_tab |
| M | install_or_shortcut_by_user_windowed | set_open_in_tab | launch_from_platform_shortcut | check_tab_not_created |
| M | install_or_shortcut_by_user_windowed | close_pwa | set_open_in_tab | launch_from_platform_shortcut | check_tab_created(One) | check_app_loaded_in_tab |
| M | install_policy_app(Standalone, ShortcutOptions::All, Windowed, WebApp) | set_open_in_tab | launch_from_platform_shortcut | check_tab_created(One) | check_app_loaded_in_tab |
| WMLC | install_or_shortcut_tabbed(NotPromotable) | launch_from_platform_shortcut(NotPromotable) | check_tab_created(One) | check_app_loaded_in_tab(NotPromotable) |
| WML | install_or_shortcut_tabbed(NotPromotable) | launch_from_chrome_apps(NotPromotable) | check_tab_not_created | check_app_loaded_in_tab(NotPromotable) |
| C | install_or_shortcut_tabbed(NotPromotable) | launch_from_chrome_apps(NotPromotable) | check_tab_created(One) | check_app_loaded_in_tab(NotPromotable) |
| WMLC | install_or_shortcut_windowed(MinimalUi) | launch(MinimalUi) | check_window_display_minimal |
| WMLC | install_or_shortcut_windowed(Tabbed) | maybe_close_pwa | launch(Tabbed) | check_window_display_tabbed |
| WMLC | install_or_shortcut_windowed(NotPromotable) | launch_not_from_platform_shortcut(NotPromotable) | check_window_created |
| WLC | install_or_shortcut_windowed(NotPromotable) | launch_from_platform_shortcut(NotPromotable) | check_window_created |
| M | install_or_shortcut_by_user_windowed(NotPromotable) | launch_from_platform_shortcut(NotPromotable) | check_window_not_created |
| M | install_policy_app(NotPromotable, ShortcutOptions::All, Windowed, WebApp) | launch_from_platform_shortcut(NotPromotable) | check_window_created |

## Multi profile launches on Mac
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| M | install_or_shortcut_by_user_windowed | switch_active_profile(Profile2) | install_or_shortcut_by_user_windowed | quit_app_shim | launch_from_platform_shortcut | check_pwa_window_created_in_profile(Standalone, One, Default) | check_pwa_window_created_in_profile(Standalone, One, Profile2) |
| M | install_or_shortcut_by_user_windowed | switch_active_profile(Profile2) | install_or_shortcut_by_user_windowed | close_pwa | quit_app_shim | launch_from_platform_shortcut | check_pwa_window_created_in_profile(Standalone, One, Default) |
| M | install_or_shortcut_by_user_windowed | close_pwa | switch_active_profile(Profile2) | install_or_shortcut_by_user_windowed | quit_app_shim | launch_from_platform_shortcut | check_pwa_window_created_in_profile(Standalone, One, Profile2) |
| M | install_or_shortcut_by_user_windowed | switch_active_profile(Profile2) | install_policy_app(Standalone, ShortcutOptions::All, Windowed, WebApp) | quit_app_shim | launch_from_platform_shortcut | check_pwa_window_created_in_profile(Standalone, One, Default) |
| M | install_or_shortcut_by_user_windowed | switch_active_profile(Profile2) | install_or_shortcut_by_user_tabbed | quit_app_shim | launch_from_platform_shortcut | check_pwa_window_created_in_profile(Standalone, One, Default) |
| M | install_or_shortcut_by_user_tabbed | switch_active_profile(Profile2) | install_or_shortcut_by_user_windowed | quit_app_shim | launch_from_platform_shortcut | check_pwa_window_created_in_profile(Standalone, One, Profile2) |

# Misc UX Flows
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_no_shortcut | create_shortcuts_from_list | check_platform_shortcut_and_icon |
| WMLC | install_or_shortcut | delete_profile | check_app_list_empty |
| WMLC | install_or_shortcut | delete_profile | check_app_not_in_list |
| WMLC | install_or_shortcut_with_shortcut | delete_profile | check_platform_shortcut_not_exists |
| WMLC | install_or_shortcut_tabbed_with_shortcut | delete_platform_shortcut | create_shortcuts_from_list | launch_from_platform_shortcut | check_tab_created(One) | check_app_loaded_in_tab |
| WLC | install_or_shortcut_windowed_with_shortcut | delete_platform_shortcut | create_shortcuts_from_list | launch_from_platform_shortcut | check_window_created |
| M | install_or_shortcut_by_user_windowed_with_shortcut | delete_platform_shortcut | create_shortcuts_from_list | launch_from_platform_shortcut | check_window_not_created |
| M |  install_policy_app(Standalone, WithShortcut, Windowed, WebApp) | delete_platform_shortcut | create_shortcuts_from_list | launch_from_platform_shortcut | check_window_created |
| WMLC | install_tabbed_no_shortcut | create_shortcuts_from_list | launch_from_platform_shortcut | check_tab_created(One) | check_app_loaded_in_tab |
| WMLC | install_windowed_no_shortcut | create_shortcuts_from_list | launch_from_platform_shortcut | check_window_created |
| WMLC | install_or_shortcut_by_user_windowed | open_in_chrome | check_tab_created(One) | check_app_loaded_in_tab |
| WMLC | install_or_shortcut_by_user_windowed | navigate_pwa(Standalone, MinimalUi) | open_in_chrome | check_tab_created(One) |
| WML | install_or_shortcut_windowed | open_app_settings | check_browser_navigation_is_app_settings |

## Sync-initiated install tests
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WML | install_or_shortcut_by_user | switch_profile_clients | install_locally | check_platform_shortcut_and_icon |
| WML | install_or_shortcut_by_user_tabbed | switch_profile_clients | install_locally | check_app_in_list_tabbed |
| WML | install_or_shortcut_by_user_tabbed | switch_profile_clients | install_locally | navigate_browser(Standalone) | check_install_icon_shown |
| WML | install_or_shortcut_by_user_tabbed | switch_profile_clients | install_locally | navigate_browser(Standalone) | check_launch_icon_not_shown |
| WML | install_or_shortcut_by_user_windowed | switch_profile_clients | install_locally | check_app_in_list_windowed |
| WML | install_or_shortcut_by_user_windowed | switch_profile_clients | install_locally | navigate_browser(Standalone) | check_install_icon_not_shown |
| WML | install_or_shortcut_by_user_windowed | switch_profile_clients | install_locally | navigate_browser(Standalone) | check_launch_icon_shown |
| WML | install_or_shortcut_by_user_tabbed(NotPromotable) | switch_profile_clients | install_locally(NotPromotable) | check_app_in_list_tabbed(NotPromotable) |
| WML | install_or_shortcut_by_user_tabbed(NotPromotable) | switch_profile_clients | install_locally(NotPromotable) | navigate_browser(NotPromotable) | check_launch_icon_not_shown |
| WML | install_or_shortcut_by_user_windowed(NotPromotable) | switch_profile_clients | install_locally(NotPromotable) | check_app_in_list_windowed(NotPromotable) |
| WML | install_or_shortcut_by_user_windowed(NotPromotable) | switch_profile_clients | install_locally(NotPromotable) | navigate_browser(NotPromotable) | check_install_icon_not_shown |
| WML | install_or_shortcut_by_user_windowed(NotPromotable) | switch_profile_clients | install_locally(NotPromotable) | navigate_browser(NotPromotable) | check_launch_icon_shown |
| WML | install_or_shortcut_by_user(NotPromotable) | switch_profile_clients | install_locally(NotPromotable) | check_platform_shortcut_and_icon(NotPromotable) |
| WML | install_or_shortcut_by_user_windowed | switch_profile_clients | install_locally | launch_not_from_platform_shortcut | check_window_created |
| WL | install_or_shortcut_by_user_windowed | switch_profile_clients | install_locally | launch_from_platform_shortcut | check_window_created |
| M | install_or_shortcut_by_user_windowed | switch_profile_clients | install_locally | launch_from_platform_shortcut | check_window_not_created |
| M | install_or_shortcut_by_user_windowed | close_pwa | switch_profile_clients | install_locally | launch_from_platform_shortcut | check_window_created |
| WML | install_or_shortcut_by_user_tabbed | switch_profile_clients | launch_from_chrome_apps | check_tab_not_created | check_app_loaded_in_tab |
| C | install_or_shortcut_by_user_tabbed | switch_profile_clients | launch_from_chrome_apps | check_tab_created(One) | check_app_loaded_in_tab |
| WML | install_or_shortcut_by_user_tabbed | switch_profile_clients | install_locally | launch_from_chrome_apps | check_tab_not_created | check_app_loaded_in_tab |
| WML | install_or_shortcut_by_user_tabbed | switch_profile_clients | install_locally | launch_from_platform_shortcut | check_tab_created(One) | check_app_loaded_in_tab |
| WML | install_or_shortcut_by_user_windowed | switch_profile_clients | launch_from_chrome_apps | check_tab_not_created | check_app_loaded_in_tab |
| WMLC | install_or_shortcut_by_user | switch_profile_clients | uninstall_from_list | check_app_not_in_list |
| WMLC | install_or_shortcut_by_user | switch_profile_clients | uninstall_from_list | switch_profile_clients(Client1) | check_app_not_in_list |
| WML | install_or_shortcut_by_user | switch_profile_clients | check_app_in_list_not_locally_installed |
| C | install_or_shortcut_by_user | switch_profile_clients | check_platform_shortcut_and_icon(Standalone) |
| WML | install_or_shortcut_by_user | switch_profile_clients | check_platform_shortcut_not_exists |
| C | install_or_shortcut_by_user_tabbed | switch_profile_clients | check_app_in_list_tabbed |
| C | install_or_shortcut_by_user_windowed | switch_profile_clients | check_app_in_list_windowed |
| C | install_or_shortcut_by_user_windowed | switch_profile_clients | navigate_browser(Standalone) | check_install_icon_not_shown |
| WML | install_or_shortcut_by_user_windowed | switch_profile_clients | navigate_browser(Standalone) | check_install_icon_shown |
| WML | install_or_shortcut_by_user_windowed | switch_profile_clients | navigate_browser(Standalone) | check_launch_icon_not_shown |
| C | install_or_shortcut_by_user_windowed | switch_profile_clients | navigate_browser(Standalone) | check_launch_icon_shown |
| WML | install_or_shortcut_by_user(NotPromotable) | switch_profile_clients | check_app_in_list_not_locally_installed(NotPromotable) |
| WML | install_or_shortcut_by_user(NotPromotable) | switch_profile_clients | check_platform_shortcut_not_exists(NotPromotable) |
| WMLC | sync_turn_off | install_or_shortcut_by_user | sync_turn_on | switch_profile_clients | check_app_not_in_list |
| WML | sync_sign_out | install_or_shortcut_by_user | sync_sign_in | switch_profile_clients | check_app_not_in_list |
| WML | install_or_shortcut_by_user | switch_profile_clients(Client2) | sync_turn_off | check_app_not_in_list | sync_turn_on | check_app_in_list_not_locally_installed |
| WML | install_or_shortcut_by_user | switch_profile_clients(Client2) | sync_turn_off | check_app_not_in_list | sync_turn_on | check_platform_shortcut_not_exists |
| WML | install_or_shortcut_by_user | switch_profile_clients(Client2) | sync_turn_off | switch_profile_clients(Client1) | uninstall_by_user | sync_turn_on | switch_profile_clients(Client2) | check_app_in_list_not_locally_installed |
| C | install_or_shortcut_by_user_tabbed | switch_profile_clients(Client2) | sync_turn_off | check_app_not_in_list | sync_turn_on | check_app_in_list_tabbed |
| C | install_or_shortcut_by_user_tabbed | switch_profile_clients(Client2) | sync_turn_off | check_app_not_in_list | sync_turn_on | check_platform_shortcut_and_icon |
| C | install_or_shortcut_by_user_windowed | switch_profile_clients(Client2) | sync_turn_off | check_app_not_in_list | sync_turn_on | check_app_in_list_windowed |
| C | install_or_shortcut_by_user_windowed | switch_profile_clients(Client2) | sync_turn_off | check_app_not_in_list | sync_turn_on | check_platform_shortcut_and_icon |

## Policy installation and user installation interactions
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_or_shortcut_by_user_tabbed | install_policy_app(Standalone, ShortcutOptions::All, Windowed, WebApp) | check_platform_shortcut_and_icon |
| WMLC | install_or_shortcut_by_user_windowed | install_policy_app(Standalone, NoShortcut, WindowOptions::All, WebApp) | check_platform_shortcut_and_icon |
| WMLC | install_or_shortcut_by_user_windowed | install_policy_app(Standalone, ShortcutOptions::All, Browser, WebApp) | check_app_in_list_windowed |
| WMLC | install_or_shortcut_by_user_windowed | install_policy_app(Standalone, ShortcutOptions::All, Browser, WebApp) | navigate_browser(Standalone) | check_launch_icon_shown |
| WMLC | install_or_shortcut_by_user_tabbed | install_policy_app(Standalone, ShortcutOptions::All, Windowed, WebApp) | check_app_in_list_tabbed |
| WMLC | install_or_shortcut_by_user_tabbed | install_policy_app(Standalone, ShortcutOptions::All, Windowed, WebApp) | navigate_browser(Standalone) | check_install_icon_shown |
| WMLC | install_or_shortcut_by_user_windowed | install_policy_app(Standalone, ShortcutOptions::All, Browser, WebApp) | launch_not_from_platform_shortcut | check_window_created |
| WLC | install_or_shortcut_by_user_windowed | install_policy_app(Standalone, ShortcutOptions::All, Browser, WebApp) | launch_from_platform_shortcut | check_window_created |
| M | install_or_shortcut_by_user_windowed | install_policy_app(Standalone, ShortcutOptions::All, Browser, WebApp) | launch_from_platform_shortcut | check_window_not_created |
| WMLC | install_policy_app(Standalone, ShortcutOptions::All, Browser, WebApp) | install_or_shortcut_by_user_windowed | check_app_in_list_windowed |
| WMLC | install_policy_app(Standalone, ShortcutOptions::All, Browser, WebApp) | install_or_shortcut_by_user_windowed | check_platform_shortcut_and_icon |
| WMLC | install_policy_app(Standalone, ShortcutOptions::All, Browser, WebApp) | install_or_shortcut_by_user_windowed | check_window_created |
| WMLC | install_or_shortcut_by_user_tabbed | install_policy_app(Standalone, ShortcutOptions::All, Windowed, WebApp) | launch_from_platform_shortcut | check_tab_created(One) | check_app_loaded_in_tab |
| WML | install_or_shortcut_by_user_tabbed | install_policy_app(Standalone, ShortcutOptions::All, Windowed, WebApp) | launch_from_chrome_apps | check_tab_not_created | check_app_loaded_in_tab |
| C | install_or_shortcut_by_user_tabbed | install_policy_app(Standalone, ShortcutOptions::All, Windowed, WebApp) | launch_from_chrome_apps | check_tab_created(One) | check_app_loaded_in_tab |
| WMLC | install_or_shortcut_by_user_tabbed | install_policy_app(Standalone, ShortcutOptions::All, WindowOptions::All, WebApp) | uninstall_policy_app | check_app_in_list_tabbed |
| WMLC | install_or_shortcut_by_user_tabbed | install_policy_app(Standalone, ShortcutOptions::All, WindowOptions::All, WebApp) | uninstall_policy_app | check_platform_shortcut_and_icon |
| WMLC | install_or_shortcut_by_user_windowed | install_policy_app(Standalone, ShortcutOptions::All, WindowOptions::All, WebApp) | uninstall_policy_app | check_app_in_list_windowed |
| WMLC | install_or_shortcut_by_user_windowed | install_policy_app(Standalone, ShortcutOptions::All, WindowOptions::All, WebApp) | uninstall_policy_app | check_platform_shortcut_and_icon |
| WMLC | install_policy_app(Standalone, ShortcutOptions::All, Browser, WebApp) | install_or_shortcut_by_user_windowed | uninstall_policy_app | check_app_in_list_windowed |
| WMLC | install_policy_app(Standalone, ShortcutOptions::All, Browser, WebApp) | install_or_shortcut_by_user_windowed | uninstall_policy_app | check_platform_shortcut_and_icon |
| WMLC | install_policy_app(StandaloneNotStartUrl, WithShortcut, Windowed, WebShortcut) | launch(StandaloneNotStartUrl) | check_app_navigation(StandaloneNotStartUrl) |
| WMLC | install_policy_app(StandaloneNotStartUrl, WithShortcut, Windowed, WebApp) | launch(Standalone) | check_app_navigation(Standalone) |
| WMLC | install_policy_app(StandaloneNotStartUrl, WithShortcut, Browser, WebApp) | launch_from_chrome_apps(Standalone) | check_browser_navigation(Standalone) |
| WMLC | install_policy_app(StandaloneNotStartUrl, WithShortcut, Browser, WebApp) | launch_from_platform_shortcut(Standalone) | check_browser_navigation(Standalone) |
| WMLC | install_policy_app(StandaloneNotStartUrl, WithShortcut, Browser, WebShortcut) | launch_from_chrome_apps(StandaloneNotStartUrl) | check_browser_navigation(StandaloneNotStartUrl) |
| WMLC | install_policy_app(StandaloneNotStartUrl, WithShortcut, Browser, WebShortcut) | launch_from_platform_shortcut(StandaloneNotStartUrl) | check_browser_navigation(StandaloneNotStartUrl) |
| WMLC | install_policy_app(StandaloneNotStartUrl, WithShortcut, WindowOptions::All, WebApp) | check_app_not_in_list(StandaloneNotStartUrl) | check_app_in_list_icon_correct(Standalone) |
| WMLC | install_policy_app(StandaloneNotStartUrl, WithShortcut, WindowOptions::All, WebShortcut) | check_app_not_in_list(Standalone) | check_app_in_list_icon_correct(StandaloneNotStartUrl) |

## Manifest update tests

Note: Updating display to "browser" means the default windowed experience is now "minimal-ui", if the user preference is still 'open in a window'.

| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WLC | install_or_shortcut_windowed_with_shortcut | manifest_update_colors | await_manifest_update | launch | check_window_color_correct |
| WLC  | install_or_shortcut_windowed_with_shortcut | manifest_update_display(Standalone, Browser) | await_manifest_update | launch | check_window_created | check_tab_not_created | check_window_display_minimal |
| WLC  | install_or_shortcut_windowed_with_shortcut | manifest_update_display(Standalone, MinimalUi) | await_manifest_update | launch | check_window_created | check_tab_not_created | check_window_display_minimal |
| WMLC | install_or_shortcut_windowed_with_shortcut(StandaloneNestedA) | manifest_update_scope_to(StandaloneNestedA, Standalone) | await_manifest_update(StandaloneNestedA) | navigate_browser(Standalone) | check_install_icon_not_shown |
| WMLC | install_or_shortcut_windowed_with_shortcut(StandaloneNestedA) | manifest_update_scope_to(StandaloneNestedA, Standalone) | await_manifest_update(StandaloneNestedA) | navigate_browser(Standalone) | check_launch_icon_shown |
| WMLC | install_or_shortcut_windowed_with_shortcut(StandaloneNestedA) | manifest_update_scope_to(StandaloneNestedA, Standalone) | await_manifest_update(StandaloneNestedA) | launch(StandaloneNestedA) | navigate_pwa(StandaloneNestedA, StandaloneNestedB) | check_no_toolbar |
| WMLC | install_or_shortcut_windowed_with_shortcut(StandaloneNestedA) | manifest_update_scope_to(StandaloneNestedA, Standalone) | await_manifest_update(StandaloneNestedA) | navigate_browser(StandaloneNestedB) | check_install_icon_not_shown |
| WMLC | install_or_shortcut_windowed_with_shortcut(StandaloneNestedA) | manifest_update_scope_to(StandaloneNestedA, Standalone) | await_manifest_update(StandaloneNestedA) | navigate_browser(StandaloneNestedB) | check_launch_icon_shown |
| WMLC | install_or_shortcut_windowed_with_shortcut(StandaloneNestedA) | manifest_update_scope_to(StandaloneNestedA, Standalone) | await_manifest_update(StandaloneNestedA) | navigate_browser(StandaloneNestedA) | check_install_icon_not_shown |
| WMLC | install_or_shortcut_windowed_with_shortcut(StandaloneNestedA) | manifest_update_scope_to(StandaloneNestedA, Standalone) | await_manifest_update(StandaloneNestedA) | navigate_browser(StandaloneNestedA) | check_launch_icon_shown |

The following specialization is required here since in tabbed mode, launching may add a tab to the existing window instead of making a new one.

| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC  | install_or_shortcut_windowed_with_shortcut | manifest_update_display(Standalone, Tabbed) | maybe_close_pwa | await_manifest_update | launch | check_window_created | check_tab_not_created | check_window_display_tabbed |

These mac specializations are required due to launching from platform shortcut actually focusing the window, instead of creating a new one.

| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| M    | install_or_shortcut_windowed_with_shortcut | manifest_update_display(Standalone, Browser) | maybe_close_pwa | await_manifest_update | launch | check_window_created | check_tab_not_created | check_window_display_minimal |
| M    | install_or_shortcut_windowed_with_shortcut | manifest_update_display(Standalone, MinimalUi) | maybe_close_pwa | await_manifest_update | launch | check_window_created | check_tab_not_created | check_window_display_minimal |

## Browser UX with edge cases
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | switch_incognito_profile | navigate_browser(Standalone) | check_create_shortcut_not_shown |
| WMLC | switch_incognito_profile | navigate_browser(NotPromotable) | check_create_shortcut_not_shown |
| WMLC | switch_incognito_profile | navigate_browser(Standalone) | check_install_icon_not_shown |
| WMLC | switch_incognito_profile | navigate_browser(NotPromotable) | check_install_icon_not_shown |
| WMLC | switch_incognito_profile | navigate_app_home | check_browser_not_at_app_home |
| WMLC | navigate_crashed_url | check_create_shortcut_not_shown |
| WMLC | navigate_crashed_url | check_install_icon_not_shown |
| WMLC | navigate_notfound_url | check_create_shortcut_not_shown |
| WMLC | navigate_notfound_url | check_install_icon_not_shown |

## Site promotability checking
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | navigate_browser(Standalone) | check_create_shortcut_shown |
| WMLC | navigate_browser(StandaloneNestedA) | check_install_icon_shown |
| WMLC | navigate_browser(NotPromotable) | check_create_shortcut_shown |
| WMLC | navigate_browser(NotPromotable) | check_install_icon_not_shown |

## In-Browser UX (install icon, launch icon, etc)
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_or_shortcut_windowed(StandaloneNestedA) | navigate_browser(NotInstalled) | check_install_icon_shown |
| WMLC | install_or_shortcut_windowed(StandaloneNestedA) | navigate_browser(NotInstalled) | check_launch_icon_not_shown |
| WMLC | install_or_shortcut_windowed | navigate_browser(StandaloneNestedA) | check_install_icon_not_shown |
| WMLC | install_or_shortcut_windowed | navigate_browser(StandaloneNestedA) | check_launch_icon_shown |
| WMLC | install_or_shortcut_by_user_windowed | navigate_browser(MinimalUi) | check_install_icon_shown |
| WMLC | install_or_shortcut_by_user_windowed | navigate_browser(MinimalUi) | check_launch_icon_not_shown |
| WMLC | install_or_shortcut_by_user_windowed | navigate_pwa(Standalone, MinimalUi) | check_app_title(Standalone, StandaloneOriginal) |
| WMLC | install_or_shortcut_by_user_windowed | switch_incognito_profile | navigate_browser(Standalone) | check_launch_icon_not_shown |
| WMLC | install_or_shortcut_by_user_windowed | set_open_in_tab | check_app_in_list_tabbed |
| WMLC | install_or_shortcut_by_user_windowed | set_open_in_tab | navigate_browser(Standalone) | check_install_icon_shown |
| WMLC | install_or_shortcut_by_user_tabbed | set_open_in_window | check_app_in_list_windowed |
| WMLC | install_or_shortcut_by_user_tabbed | set_open_in_window | navigate_browser(Standalone) | check_install_icon_not_shown |
| WMLC | install_or_shortcut_by_user_tabbed | set_open_in_window | navigate_browser(Standalone) | check_launch_icon_shown |

## Windows Control Overlay
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_or_shortcut_by_user_windowed(Wco) | check_window_controls_overlay_toggle(Wco, Shown) |
| WMLC | install_policy_app(Wco, ShortcutOptions::All, Windowed, WebApp) | launch(Wco) | check_window_controls_overlay_toggle(Wco, Shown) |
| WMLC | install_or_shortcut_by_user_windowed(Standalone) | check_window_controls_overlay_toggle(Standalone, NotShown) |
| WMLC | install_policy_app(Standalone, ShortcutOptions::All, Windowed, WebApp) | launch(Standalone) | check_window_controls_overlay_toggle(Standalone, NotShown) |
| WMLC | install_or_shortcut_by_user_windowed(Wco) | enable_window_controls_overlay(Wco) | check_window_controls_overlay(Wco, On) |
| WMLC | install_policy_app(Wco, ShortcutOptions::All, Windowed, WebApp) | launch(Wco) | enable_window_controls_overlay(Wco) | check_window_controls_overlay(Wco, On) |
| WMLC | install_or_shortcut_by_user_windowed(Wco) | enable_window_controls_overlay(Wco) | check_window_controls_overlay_toggle(Wco, Shown) |
| WMLC | install_policy_app(Wco, ShortcutOptions::All, Windowed, WebApp) | launch(Wco) | enable_window_controls_overlay(Wco) | check_window_controls_overlay_toggle(Wco, Shown) |
| WMLC | install_or_shortcut_by_user_windowed(Wco) | enable_window_controls_overlay(Wco) | disable_window_controls_overlay(Wco) | check_window_controls_overlay(Wco, Off) |
| WMLC | install_policy_app(Wco, ShortcutOptions::All, Windowed, WebApp) | launch(Wco) | enable_window_controls_overlay(Wco) | disable_window_controls_overlay(Wco) | check_window_controls_overlay(Wco, Off) |
| WMLC | install_or_shortcut_by_user_windowed(Wco) | enable_window_controls_overlay(Wco) | disable_window_controls_overlay(Wco) | check_window_controls_overlay_toggle(Wco, Shown) |
| WMLC | install_policy_app(Wco, ShortcutOptions::All, Windowed, WebApp) | launch(Wco) | enable_window_controls_overlay(Wco) | disable_window_controls_overlay(Wco) | check_window_controls_overlay_toggle(Wco, Shown) |
| WMLC | install_or_shortcut_by_user_windowed(Wco) | enable_window_controls_overlay(Wco) | launch(Wco) | check_window_controls_overlay(Wco, On) |
| WMLC | install_policy_app(Wco, ShortcutOptions::All, Windowed, WebApp) | launch(Wco) | enable_window_controls_overlay(Wco) | launch(Wco) | check_window_controls_overlay(Wco, On) |
| WMLC | install_or_shortcut_windowed(MinimalUi) | manifest_update_display(MinimalUi, Wco) | await_manifest_update(MinimalUi) | maybe_close_pwa | launch(MinimalUi) | check_window_controls_overlay_toggle(MinimalUi, Shown) |
| WMLC | install_or_shortcut_windowed(MinimalUi) | manifest_update_display(MinimalUi, Wco) | await_manifest_update(MinimalUi) | maybe_close_pwa | launch(MinimalUi) | check_window_controls_overlay_toggle(MinimalUi, Shown) |
| WMLC | install_or_shortcut_windowed(MinimalUi) | manifest_update_display(MinimalUi, Wco) | await_manifest_update(MinimalUi) | maybe_close_pwa | launch(MinimalUi) | enable_window_controls_overlay(MinimalUi) | check_window_controls_overlay(MinimalUi, On) |
| WMLC | install_or_shortcut_windowed(MinimalUi) | manifest_update_display(MinimalUi, Wco) | await_manifest_update(MinimalUi) | maybe_close_pwa | launch(MinimalUi) | enable_window_controls_overlay(MinimalUi) | check_window_controls_overlay_toggle(MinimalUi, Shown) |
| WMLC | install_or_shortcut_windowed(MinimalUi) | manifest_update_display(MinimalUi, Wco) | await_manifest_update(MinimalUi) | maybe_close_pwa | launch(MinimalUi) | enable_window_controls_overlay(MinimalUi) | check_window_controls_overlay_toggle(MinimalUi, Shown) |
| WMLC | install_or_shortcut_windowed(Wco) | manifest_update_display(Wco, Standalone) | await_manifest_update(Wco) | maybe_close_pwa | launch(Wco) | check_window_controls_overlay_toggle(Wco, NotShown) |
| WMLC | install_or_shortcut_windowed(Wco) | manifest_update_display(Wco, Standalone) | await_manifest_update(Wco) | maybe_close_pwa | launch(Wco) | check_window_controls_overlay(Wco, Off) |
| WMLC | install_or_shortcut_by_user_windowed(Wco) |  check_window_controls_overlay_toggle_icon(Shown) |
| WMLC | install_or_shortcut_by_user_windowed(Wco) |  enter_full_screen_app | check_window_controls_overlay_toggle_icon(NotShown) |
| WMLC | install_or_shortcut_by_user_windowed(Wco) |  enter_full_screen_app | exit_full_screen_app | check_window_controls_overlay_toggle_icon(Shown) |
| WMLC | install_policy_app(Wco, ShortcutOptions::All, Windowed, WebApp) | launch(Wco) | check_window_controls_overlay_toggle_icon(Shown) |
| WMLC | install_policy_app(Wco, ShortcutOptions::All, Windowed, WebApp) | launch(Wco) | enter_full_screen_app | check_window_controls_overlay_toggle_icon(NotShown) |
| WMLC | install_policy_app(Wco, ShortcutOptions::All, Windowed, WebApp) | launch(Wco) | enter_full_screen_app | exit_full_screen_app | check_window_controls_overlay_toggle_icon(Shown) |

## File Handling

| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_or_shortcut(FileHandler) | check_site_handles_file(FileHandler, Foo) | check_site_handles_file(FileHandler, Bar) |

### Launching a single window or tab vs multiple

The test behavior can change whether the site is configured to open as a window or open as a tab. To achieve this, we use three actions to enter those states:
- `install_or_shortcut_windowed`
- `install_or_shortcut_tabbed`
- `install_or_shortcut_windowed` + `set_open_in_tab`

| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| # Single file opens should open just one window or tab. |
| WMLC | install_or_shortcut_windowed(FileHandler) | launch_file_expect_dialog(FileHandler, OneFooFile, Allow, AskAgain) | check_pwa_window_created(FileHandler, One) | check_files_loaded_in_site(FileHandler, OneFooFile) |
| WMLC | install_or_shortcut_tabbed(FileHandler) | launch_file_expect_dialog(FileHandler, OneFooFile, Allow, AskAgain) | check_tab_created(One) | check_files_loaded_in_site(FileHandler, OneFooFile) |
| WMLC | install_or_shortcut_windowed(FileHandler) | set_open_in_tab(FileHandler) | launch_file_expect_dialog(FileHandler, OneFooFile, Allow, AskAgain) | check_tab_created(One) | check_files_loaded_in_site(FileHandler, OneFooFile) |
| WMLC | install_or_shortcut_windowed(FileHandler) | launch_file_expect_dialog(FileHandler, OneBarFile, Allow, AskAgain) | check_pwa_window_created(FileHandler, One) | check_files_loaded_in_site(FileHandler, OneBarFile) |
| WMLC | install_or_shortcut_tabbed(FileHandler) | launch_file_expect_dialog(FileHandler, OneBarFile, Allow, AskAgain) | check_tab_created(One) | check_files_loaded_in_site(FileHandler, OneBarFile) |
| WMLC | install_or_shortcut_windowed(FileHandler) | set_open_in_tab(FileHandler) | launch_file_expect_dialog(FileHandler, OneBarFile, Allow, AskAgain) | check_tab_created(One) | check_files_loaded_in_site(FileHandler, OneBarFile) |
| # Opening multiple Foo files only opens one window or tab. |
| WMLC | install_or_shortcut_windowed(FileHandler) | launch_file_expect_dialog(FileHandler, MultipleFooFiles, Allow, AskAgain) | check_pwa_window_created(FileHandler, One) | check_files_loaded_in_site(FileHandler, MultipleFooFiles) |
| WMLC | install_or_shortcut_tabbed(FileHandler) | launch_file_expect_dialog(FileHandler, MultipleFooFiles, Allow, AskAgain) | check_tab_created(One) | check_files_loaded_in_site(FileHandler, MultipleFooFiles) |
| WMLC | install_or_shortcut_windowed(FileHandler) | set_open_in_tab(FileHandler) | launch_file_expect_dialog(FileHandler, MultipleFooFiles, Allow, AskAgain) | check_tab_created(One) | check_files_loaded_in_site(FileHandler, MultipleFooFiles) |
| # Opening multiple Bar files opens multiple windows or tabs. |
| WMLC | install_or_shortcut_windowed(FileHandler) | launch_file_expect_dialog(FileHandler, MultipleBarFiles, Allow, AskAgain) | check_pwa_window_created(FileHandler, Two) | check_files_loaded_in_site(FileHandler, MultipleBarFiles) |
| WMLC | install_or_shortcut_tabbed(FileHandler) | launch_file_expect_dialog(FileHandler, MultipleBarFiles, Allow, AskAgain) | check_tab_created(Two) | check_files_loaded_in_site(FileHandler, MultipleBarFiles) |
| WMLC | install_or_shortcut_windowed(FileHandler) | set_open_in_tab(FileHandler) | launch_file_expect_dialog(FileHandler, MultipleBarFiles, Allow, AskAgain) | check_tab_created(Two) | check_files_loaded_in_site(FileHandler, MultipleBarFiles) |

### Multi-profile behavior

| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| # To slightly reduce number of generated tests, and work around test name limitations, separate out some by_user and policy install cases.
| # Launch a file in the primary profile, while the PWA is not currently open.
| WML | install_or_shortcut_by_user_windowed(FileHandler) | maybe_close_pwa | switch_active_profile(Profile2) | install_or_shortcut(FileHandler) | maybe_close_pwa | disable_file_handling(FileHandler) | switch_active_profile(Default) | launch_file_expect_dialog(FileHandler, OneFooFile, Allow, AskAgain) | check_pwa_window_created(FileHandler, One) | check_files_loaded_in_site(FileHandler, OneFooFile) |
| WML | install_or_shortcut_by_user_tabbed(FileHandler) | switch_active_profile(Profile2) | install_or_shortcut(FileHandler) | maybe_close_pwa | disable_file_handling(FileHandler) | switch_active_profile(Default) | launch_file_expect_dialog(FileHandler, OneFooFile, Allow, AskAgain) | check_tab_created(One) | check_files_loaded_in_site(FileHandler, OneFooFile) |
| # Launch a file in the primary profile, while the PWA is open in the secondary profile.
| WML | install_or_shortcut_by_user_windowed(FileHandler) | maybe_close_pwa | switch_active_profile(Profile2) | install_or_shortcut(FileHandler) | disable_file_handling(FileHandler) | switch_active_profile(Default) | launch_file_expect_dialog(FileHandler, OneFooFile, Allow, AskAgain) | check_pwa_window_created(FileHandler, One) | check_files_loaded_in_site(FileHandler, OneFooFile) |
| WML | install_or_shortcut_by_user_tabbed(FileHandler) | switch_active_profile(Profile2) | install_or_shortcut(FileHandler) | disable_file_handling(FileHandler) | switch_active_profile(Default) | launch_file_expect_dialog(FileHandler, OneFooFile, Allow, AskAgain) | check_tab_created(One) | check_files_loaded_in_site(FileHandler, OneFooFile) |
| WML | install_policy_app(FileHandler, ShortcutOptions::All, Windowed, WebApp) | switch_active_profile(Profile2) | install_or_shortcut(FileHandler) | disable_file_handling(FileHandler) | switch_active_profile(Default) | launch_file_expect_dialog(FileHandler, OneFooFile, Allow, AskAgain) | check_pwa_window_created(FileHandler, One) | check_files_loaded_in_site(FileHandler, OneFooFile) |
| WML | install_policy_app(FileHandler, ShortcutOptions::All, Browser, WebApp) | switch_active_profile(Profile2) | install_or_shortcut(FileHandler) | disable_file_handling(FileHandler) | switch_active_profile(Default) | launch_file_expect_dialog(FileHandler, OneFooFile, Allow, AskAgain) | check_tab_created(One) | check_files_loaded_in_site(FileHandler, OneFooFile) |


### Dialog option

| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_or_shortcut_windowed(FileHandler) | launch_file_expect_dialog(FileHandler, OneFooFile, Allow, Remember) | launch_file_expect_no_dialog(FileHandler, OneFooFile) |
| WMLC | install_or_shortcut_windowed(FileHandler) | launch_file_expect_dialog(FileHandler, OneFooFile, Allow, Remember) | close_pwa | launch_file_expect_no_dialog(FileHandler, OneFooFile) | check_files_loaded_in_site(FileHandler, OneFooFile) |
| WMLC | install_or_shortcut_windowed(FileHandler) | launch_file_expect_dialog(FileHandler, OneFooFile, Allow, Remember) | close_pwa | launch_file_expect_no_dialog(FileHandler, OneFooFile) | close_pwa | launch_file_expect_no_dialog(FileHandler, OneBarFile) | check_files_loaded_in_site(FileHandler, OneBarFile) |
| WMLC | install_or_shortcut(FileHandler) | launch_file_expect_dialog(FileHandler, OneFooFile, Allow, AskAgain) | launch_file_expect_dialog(FileHandler, OneFooFile, Allow, AskAgain) |
| WMLC | install_or_shortcut(FileHandler) | launch_file_expect_dialog(FileHandler, OneFooFile, Deny, AskAgain) | check_window_not_created | check_tab_not_created | check_site_handles_file(FileHandler, Foo) | check_site_handles_file(FileHandler, Bar) |
| WMLC | install_or_shortcut(FileHandler) | launch_file_expect_dialog(FileHandler, OneFooFile, Deny, AskAgain) | launch_file_expect_dialog(FileHandler, OneFooFile, Allow, AskAgain) | check_files_loaded_in_site(FileHandler, OneFooFile) |
| WMLC | install_or_shortcut(FileHandler) | launch_file_expect_dialog(FileHandler, OneFooFile, Deny, Remember) | check_window_not_created | check_tab_not_created | check_site_not_handles_file(FileHandler, Foo) | check_site_not_handles_file(FileHandler, Bar) |

### Policy test for forcing file handling approval

| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_or_shortcut(FileHandler) | add_file_handling_policy_approval(FileHandler) | launch_file_expect_no_dialog(FileHandler, OneFooFile) | check_pwa_window_created(FileHandler, One) |
| WMLC | install_or_shortcut(FileHandler) | add_file_handling_policy_approval(FileHandler) | remove_file_handling_policy_approval(FileHandler) | launch_file_expect_dialog(FileHandler, OneFooFile, Allow, AskAgain) |


## Sub Apps

| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| #C | install_or_shortcut(HasSubApps) | install_sub_app(HasSubApps, SubApp1, UserAllow) | check_app_in_list_windowed(SubApp1) | check_has_sub_app(HasSubApps, SubApp1) | check_platform_shortcut_and_icon(SubApp1) |
| #C | install_or_shortcut(HasSubApps) | install_sub_app(HasSubApps, SubApp1, UserDeny) | check_app_not_in_list(SubApp1) | check_platform_shortcut_not_exists(SubApp1) |
| #C | install_or_shortcut_by_user(HasSubApps) | install_sub_app(HasSubApps, SubApp1, UserAllow) | uninstall_by_user(HasSubApps) | check_app_not_in_list(SubApp1) | check_platform_shortcut_not_exists(SubApp1) |
| #C | install_policy_app(HasSubApps, ShortcutOptions::All, WindowOptions::All, WebApp) | install_sub_app(HasSubApps, SubApp1, UserAllow) | uninstall_policy_app(HasSubApps) | check_app_not_in_list(SubApp1) | check_platform_shortcut_not_exists(SubApp1) |
| #C | install_or_shortcut(HasSubApps) | install_sub_app(HasSubApps, SubApp1, UserAllow) | install_sub_app(HasSubApps, SubApp2, UserAllow) | check_has_sub_app(HasSubApps, SubApp1) | check_has_sub_app(HasSubApps, SubApp2) |
| #C | install_or_shortcut(HasSubApps) | check_no_sub_apps(HasSubApps) |
| #C | install_or_shortcut(HasSubApps) | install_sub_app(HasSubApps, SubApp1, UserAllow) | remove_sub_app(HasSubApps, SubApp1) | check_app_not_in_list(SubApp1) | check_platform_shortcut_not_exists(SubApp1) | check_no_sub_apps(HasSubApps) |
| #C | install_or_shortcut_windowed(SubApp1) | check_app_in_list_windowed(SubApp1) | check_platform_shortcut_and_icon(SubApp1)
| #C | install_or_shortcut_tabbed(SubApp1) | check_app_in_list_tabbed(SubApp1) | check_platform_shortcut_and_icon(SubApp1)
| #C | install_or_shortcut(SubApp1) | install_or_shortcut(HasSubApps) | check_not_has_sub_app(HasSubApps, SubApp1)
| #C | install_or_shortcut(SubApp1) | install_or_shortcut(HasSubApps) | install_sub_app(HasSubApps, SubApp1, UserAllow) | check_has_sub_app(HasSubApps, SubApp1)
| #C | install_or_shortcut_windowed(SubApp1) | install_or_shortcut(HasSubApps) | install_sub_app(HasSubApps, SubApp1, UserAllow) | remove_sub_app(HasSubApps, SubApp1) | check_not_has_sub_app(HasSubApps, SubApp1) | check_app_in_list_windowed(SubApp1) | check_platform_shortcut_and_icon(SubApp1)
| #C | install_or_shortcut_tabbed(SubApp1) | install_or_shortcut(HasSubApps) | install_sub_app(HasSubApps, SubApp1, UserAllow) | remove_sub_app(HasSubApps, SubApp1) | check_not_has_sub_app(HasSubApps, SubApp1) | check_app_in_list_tabbed(SubApp1) | check_platform_shortcut_and_icon(SubApp1)
