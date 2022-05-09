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
| WMLC | install_by_user_windowed(SiteA) | manifest_update_title(SiteA) | accept_app_id_update_dialog | close_pwa | launch(SiteA) | check_app_title_site_a_is(SiteAUpdated) |
| WMLC | install_by_user_windowed(SiteA) | manifest_update_title(SiteA) | deny_app_update_dialog | check_app_not_in_list | check_platform_shortcut_not_exists | 
| WMLC | install_by_user_windowed | close_pwa | manifest_update_icon | check_app_in_list_icon_correct |
| WMLC | install_by_user_windowed | close_pwa | manifest_update_icon | check_platform_shortcut_and_icon |
| WMLC | install_policy_app(SiteA) | manifest_update_title(SiteA) | check_update_dialog_not_shown | close_pwa | launch(SiteA) | check_app_title_site_a_is(SiteAUpdated) |

## Run on OS Login
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WML | install | apply_run_on_os_login_policy_blocked | check_user_cannot_set_run_on_os_login | 
| WML | install | enable_run_on_os_login | apply_run_on_os_login_policy_blocked | check_run_on_os_login_disabled |
| WML | install | apply_run_on_os_login_policy_run_windowed | check_run_on_os_login_enabled | 
| WML | install | apply_run_on_os_login_policy_run_windowed | check_user_cannot_set_run_on_os_login | 
| WML | install | enable_run_on_os_login | check_run_on_os_login_enabled | 
| WML | install | enable_run_on_os_login | disable_run_on_os_login | check_run_on_os_login_disabled |
| WML | install | apply_run_on_os_login_policy_run_windowed | remove_run_on_os_login_policy | check_run_on_os_login_disabled |
| WML | install | enable_run_on_os_login | apply_run_on_os_login_policy_blocked | remove_run_on_os_login_policy | check_run_on_os_login_enabled | 
| WML | install_by_user_windowed | switch_profile_clients(Client2) | switch_profile_clients(Client1) | sync_turn_off | uninstall_by_user | switch_profile_clients(Client2) | apply_run_on_os_login_policy_run_windowed | check_run_on_os_login_disabled |
| WML | install_by_user_windowed | switch_profile_clients(Client2) | switch_profile_clients(Client1) | sync_turn_off | uninstall_by_user | switch_profile_clients(Client2) | apply_run_on_os_login_policy_run_windowed | check_run_on_os_login_disabled | install_locally | check_run_on_os_login_enabled |

## Badging
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_by_user_windowed | set_app_badge | clear_app_badge | check_app_badge_empty |
| WMLC | install_by_user_windowed | set_app_badge | check_app_badge_has_value | 
| WMLC | navigate_browser(SiteA) | set_app_badge | check_platform_shortcut_not_exists | 
| # Toolbar |
| WMLC | install_windowed | navigate_pwa_site_a_to(SiteB) | close_custom_toolbar | check_app_navigation_is_start_url |
| WMLC | install_by_user_windowed | navigate_pwa_site_a_to(SiteB) | check_custom_toolbar | 
| # Initial state sanity checks |
| WMLC | navigate_browser(SiteA) | check_app_not_in_list |
| WMLC | navigate_browser(SiteA) | check_platform_shortcut_not_exists |
| WMLC | navigate_browser(SiteC) | check_app_not_in_list |
| WMLC | navigate_browser(SiteC) | check_platform_shortcut_not_exists |

# Installation
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_by_user_windowed | check_window_created |
| WMLC | install_no_shortcut | check_platform_shortcut_not_exists |
| WMLC | install_no_shortcut(SiteC) | check_platform_shortcut_not_exists(SiteC) |
| WMLC | install_tabbed | check_app_in_list_tabbed |
| WMLC | install_tabbed | navigate_browser(SiteA) | check_create_shortcut_shown | 
| WMLC | install_tabbed | navigate_browser(SiteA) | check_install_icon_shown | 
| WMLC | install_tabbed | navigate_browser(SiteA) | check_launch_icon_not_shown | 
| WMLC | install_tabbed(SiteC) | check_app_in_list_tabbed(SiteC) |
| WMLC | install_tabbed(SiteC) | navigate_browser(SiteC) | check_create_shortcut_shown | 
| WMLC | install_tabbed(SiteC) | navigate_browser(SiteC) | check_install_icon_not_shown | 
| WMLC | install_tabbed(SiteC) | navigate_browser(SiteC) | check_launch_icon_not_shown | 
| WMLC | install_windowed | check_app_in_list_windowed |
| WMLC | install_windowed | navigate_browser(SiteA) | check_create_shortcut_not_shown | 
| WMLC | install_windowed | navigate_browser(SiteA) | check_install_icon_not_shown | 
| WMLC | install_windowed | navigate_browser(SiteA) | check_launch_icon_shown | 
| WMLC | install_windowed(SiteB) | navigate_browser(SiteB) | check_launch_icon_shown | 
| WMLC | install_windowed(SiteC) | check_app_in_list_windowed(SiteC) |
| WMLC | install_windowed(SiteC) | navigate_browser(SiteC) | check_create_shortcut_not_shown | 
| WMLC | install_windowed(SiteC) | navigate_browser(SiteC) | check_install_icon_not_shown | 
| WMLC | install_windowed(SiteC) | navigate_browser(SiteC) | check_launch_icon_shown | 
| WMLC | install_with_shortcut | check_platform_shortcut_and_icon |
| WMLC | install_with_shortcut(SiteC) | check_platform_shortcut_and_icon(SiteC) |

## Uninstallation
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WML | install_by_user_windowed | uninstall_by_user | check_app_not_in_list | 
| WML | install_by_user_windowed | uninstall_by_user | navigate_browser(SiteA) | check_install_icon_shown |
| WML | install_by_user_windowed | uninstall_by_user | navigate_browser(SiteA) | check_launch_icon_not_shown |
| WML | install_by_user_windowed | uninstall_by_user | check_platform_shortcut_not_exists | 
| WML | install_by_user_windowed(SiteC) | uninstall_by_user(SiteC) | check_app_not_in_list | 
| WML | install_by_user_windowed(SiteC) | uninstall_by_user(SiteC) | check_platform_shortcut_not_exists(SiteC) | 
| WMLC | install_by_user_tabbed | uninstall_from_list | check_app_not_in_list | 
| WMLC | install_by_user_tabbed | uninstall_from_list | navigate_browser(SiteA) | check_install_icon_shown |
| WMLC | install_by_user_tabbed | uninstall_from_list | navigate_browser(SiteA) | check_launch_icon_not_shown |
| WMLC | install_by_user_tabbed | uninstall_from_list | check_platform_shortcut_not_exists | 
| C | install_by_user_windowed | uninstall_from_list | check_app_not_in_list | 
| C | install_by_user_windowed | uninstall_from_list | navigate_browser(SiteA) | check_install_icon_shown |
| C | install_by_user_windowed | uninstall_from_list | navigate_browser(SiteA) | check_launch_icon_not_shown |
| C | install_by_user_windowed | uninstall_from_list | check_platform_shortcut_not_exists | 
| WMLC | install_by_user_tabbed(SiteC) | uninstall_from_list(SiteC) | check_app_not_in_list | 
| WMLC | install_by_user_tabbed(SiteC) | uninstall_from_list(SiteC) | check_platform_shortcut_not_exists(SiteC) | 
| C | install_by_user_windowed(SiteC) | uninstall_from_list(SiteC) | check_app_not_in_list | 
| C | install_by_user_windowed(SiteC) | uninstall_from_list(SiteC) | check_platform_shortcut_not_exists(SiteC) | 
| WMLC | install_policy_app | uninstall_policy_app | check_app_not_in_list | 
| WMLC | install_policy_app_tabbed_shortcut | uninstall_policy_app | navigate_browser(SiteA) | check_install_icon_shown |
| WMLC | install_policy_app_tabbed_shortcut | uninstall_policy_app | navigate_browser(SiteA) | check_launch_icon_not_shown |
| WMLC | install_policy_app | uninstall_policy_app | check_platform_shortcut_not_exists | check_app_not_in_list |

# Launch behavior tests
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_windowed | launch | check_window_created | 
| WMLC | install_windowed | launch | check_window_display_standalone | 
| WMLC | install_tabbed | set_open_in_window | launch | check_window_created |
| WMLC | install_windowed | set_open_in_tab | launch_from_shortcut_or_list | check_tab_created |
| WMLC | install_tabbed(SiteC) | launch_from_shortcut_or_list(SiteC) | check_tab_created | 
| WMLC | install_windowed(SiteB) | launch(SiteB) | check_window_display_minimal | 
| WMLC | install_windowed(SiteC) | launch(SiteC) | check_window_created | 

# Misc UX Flows
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_no_shortcut | create_shortcuts | check_platform_shortcut_and_icon | 
| WMLC | install | delete_profile | check_app_list_empty | 
| WMLC | install | delete_profile | check_app_not_in_list | 
| WMLC | install_with_shortcut | delete_profile | check_platform_shortcut_not_exists | 
| WMLC | install_tabbed | delete_platform_shortcut | create_shortcuts | launch_from_platform_shortcut | check_tab_created | 
| WMLC | install_windowed | delete_platform_shortcut | create_shortcuts | launch_from_platform_shortcut | check_window_created | 
| WMLC | install_by_user_windowed | open_in_chrome | check_tab_created | 
| WMLC | install_by_user_windowed | navigate_pwa_site_a_to(SiteB) | open_in_chrome | check_tab_created |
| WML | install_windowed | open_app_settings | check_browser_navigation_is_app_settings | 

## Sync-initiated install tests
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WML | install_by_user | switch_profile_clients | install_locally | check_platform_shortcut_and_icon |
| WML | install_by_user_tabbed | switch_profile_clients | install_locally | check_app_in_list_tabbed |
| WML | install_by_user_tabbed | switch_profile_clients | install_locally | navigate_browser(SiteA) | check_install_icon_shown | 
| WML | install_by_user_tabbed | switch_profile_clients | install_locally | navigate_browser(SiteA) | check_launch_icon_not_shown | 
| WML | install_by_user_windowed | switch_profile_clients | install_locally | check_app_in_list_windowed |
| WML | install_by_user_windowed | switch_profile_clients | install_locally | navigate_browser(SiteA) | check_install_icon_not_shown | 
| WML | install_by_user_windowed | switch_profile_clients | install_locally | navigate_browser(SiteA) | check_launch_icon_shown | 
| WML | install_by_user_tabbed(SiteC) | switch_profile_clients | install_locally(SiteC) | check_app_in_list_tabbed(SiteC) |
| WML | install_by_user_tabbed(SiteC) | switch_profile_clients | install_locally(SiteC) | navigate_browser(SiteC) | check_launch_icon_not_shown | 
| WML | install_by_user_windowed(SiteC) | switch_profile_clients | install_locally(SiteC) | check_app_in_list_windowed(SiteC) |
| WML | install_by_user_windowed(SiteC) | switch_profile_clients | install_locally(SiteC) | navigate_browser(SiteC) | check_install_icon_not_shown | 
| WML | install_by_user_windowed(SiteC) | switch_profile_clients | install_locally(SiteC) | navigate_browser(SiteC) | check_launch_icon_shown | 
| WML | install_by_user(SiteC) | switch_profile_clients | install_locally(SiteC) | check_platform_shortcut_and_icon(SiteC) |
| WML | install_by_user_windowed | switch_profile_clients | install_locally | launch | check_window_created | 
| WMLC | install_by_user_tabbed | switch_profile_clients | launch_from_shortcut_or_list | check_tab_created |
| WML | install_by_user_tabbed | switch_profile_clients | install_locally | launch_from_shortcut_or_list | check_tab_created | 
| WML | install_by_user_windowed | switch_profile_clients | launch_from_shortcut_or_list | check_tab_created |
| WMLC | install_by_user | switch_profile_clients | uninstall_from_list | check_app_not_in_list |
| WMLC | install_by_user | switch_profile_clients | uninstall_from_list | switch_profile_clients(Client1) | check_app_not_in_list | 
| WML | install_by_user | switch_profile_clients | check_app_in_list_not_locally_installed | 
| C | install_by_user | switch_profile_clients | check_platform_shortcut_and_icon(SiteA) | 
| WML | install_by_user | switch_profile_clients | check_platform_shortcut_not_exists | 
| C | install_by_user_tabbed | switch_profile_clients | check_app_in_list_tabbed | 
| C | install_by_user_windowed | switch_profile_clients | check_app_in_list_windowed | 
| C | install_by_user_windowed | switch_profile_clients | navigate_browser(SiteA) | check_install_icon_not_shown |
| WML | install_by_user_windowed | switch_profile_clients | navigate_browser(SiteA) | check_install_icon_shown |
| WML | install_by_user_windowed | switch_profile_clients | navigate_browser(SiteA) | check_launch_icon_not_shown |
| C | install_by_user_windowed | switch_profile_clients | navigate_browser(SiteA) | check_launch_icon_shown |
| WML | install_by_user(SiteC) | switch_profile_clients | check_app_in_list_not_locally_installed(SiteC) | 
| WML | install_by_user(SiteC) | switch_profile_clients | check_platform_shortcut_not_exists(SiteC) | 
| WML | sync_turn_off | install_by_user | sync_turn_on | switch_profile_clients | check_app_in_list_not_locally_installed | 
| WML | sync_turn_off | install_by_user(SiteC) | sync_turn_on | switch_profile_clients | check_app_in_list_not_locally_installed(SiteC) | 
| WML | install_by_user | switch_profile_clients(Client2) | sync_turn_off | uninstall_not_locally_installed | sync_turn_on | check_app_in_list_not_locally_installed |
| WML | install_by_user | switch_profile_clients(Client2) | sync_turn_off | uninstall_not_locally_installed | sync_turn_on | check_platform_shortcut_not_exists |
| C | install_by_user_tabbed | switch_profile_clients(Client2) | sync_turn_off | uninstall_by_user | sync_turn_on | check_app_in_list_tabbed |
| C | install_by_user_tabbed | switch_profile_clients(Client2) | sync_turn_off | uninstall_by_user | sync_turn_on | check_platform_shortcut_and_icon |
| C | install_by_user_windowed | switch_profile_clients(Client2) | sync_turn_off | uninstall_by_user | sync_turn_on | check_app_in_list_windowed |
| C | install_by_user_windowed | switch_profile_clients(Client2) | sync_turn_off | uninstall_by_user | sync_turn_on | check_platform_shortcut_and_icon |

## Policy installation and user installation interactions
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_by_user_tabbed | install_policy_app_windowed | check_platform_shortcut_and_icon | 
| WMLC | install_by_user_windowed | install_policy_app_no_shortcut | check_platform_shortcut_and_icon | 
| WMLC | install_by_user_windowed | install_policy_app_tabbed | check_app_in_list_windowed | 
| WMLC | install_by_user_windowed | install_policy_app_tabbed | navigate_browser(SiteA) | check_launch_icon_shown |
| WMLC | install_by_user_tabbed | install_policy_app_windowed | check_app_in_list_tabbed | 
| WMLC | install_by_user_tabbed | install_policy_app_windowed | navigate_browser(SiteA) | check_install_icon_shown |
| WMLC | install_by_user_windowed | install_policy_app_tabbed | launch | check_window_created |
| WMLC | install_policy_app_tabbed | install_by_user_windowed | check_app_in_list_windowed | 
| WMLC | install_policy_app_tabbed | install_by_user_windowed | check_platform_shortcut_and_icon | 
| WMLC | install_policy_app_tabbed | install_by_user_windowed | check_window_created | 
| WMLC | install_by_user_tabbed | install_policy_app_windowed | launch_from_shortcut_or_list | check_tab_created |
| WMLC | install_by_user_tabbed | install_policy_app | uninstall_policy_app | check_app_in_list_tabbed |
| WMLC | install_by_user_tabbed | install_policy_app | uninstall_policy_app | check_platform_shortcut_and_icon |
| WMLC | install_by_user_windowed | install_policy_app | uninstall_policy_app | check_app_in_list_windowed |
| WMLC | install_by_user_windowed | install_policy_app | uninstall_policy_app | check_platform_shortcut_and_icon |
| WMLC | install_policy_app_tabbed | install_by_user_windowed | uninstall_policy_app | check_app_in_list_windowed |
| WMLC | install_policy_app_tabbed | install_by_user_windowed | uninstall_policy_app | check_platform_shortcut_and_icon |

## Manifest update tests
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_by_user_windowed | close_pwa | manifest_update_colors | launch | check_window_color_correct | 
| WMLC | install_by_user_windowed | close_pwa | manifest_update_display_browser | launch | check_tab_not_created | 
| WMLC | install_by_user_windowed | close_pwa | manifest_update_display_browser | launch | check_window_created | 
| WMLC | install_by_user_windowed | close_pwa | manifest_update_display_browser | launch | check_window_display_minimal | 
| WMLC | install_by_user_windowed | close_pwa | manifest_update_display_minimal | launch | check_window_display_minimal | 
| WMLC | install_by_user_windowed(SiteAFoo) | manifest_update_scope_site_a_foo_to(SiteARoot) | close_pwa | launch_from_platform_shortcut(SiteAFoo) | close_pwa | navigate_browser(SiteA) | check_install_icon_not_shown | 
| WMLC | install_by_user_windowed(SiteAFoo) | manifest_update_scope_site_a_foo_to(SiteARoot) | close_pwa | launch_from_platform_shortcut(SiteAFoo) | close_pwa | navigate_browser(SiteA) | check_launch_icon_shown | 
| WMLC | install_by_user_windowed(SiteAFoo) | close_pwa | manifest_update_scope_site_a_foo_to(SiteARoot) | launch(SiteAFoo) | navigate_pwa_site_a_foo_to(SiteABar) | check_no_toolbar |
| WMLC | install_by_user_windowed(SiteAFoo) | close_pwa | manifest_update_scope_site_a_foo_to(SiteARoot) | navigate_browser(SiteABar) | check_install_icon_not_shown | 
| WMLC | install_by_user_windowed(SiteAFoo) | close_pwa | manifest_update_scope_site_a_foo_to(SiteARoot) | navigate_browser(SiteABar) | check_launch_icon_shown | 
| WMLC | install_by_user_windowed(SiteAFoo) | close_pwa | manifest_update_scope_site_a_foo_to(SiteARoot) | navigate_browser(SiteAFoo) | check_install_icon_not_shown | 
| WMLC | install_by_user_windowed(SiteAFoo) | close_pwa | manifest_update_scope_site_a_foo_to(SiteARoot) | navigate_browser(SiteAFoo) | check_launch_icon_shown | 

## Browser UX with edge cases
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | switch_incognito_profile | navigate_browser(SiteA) | check_create_shortcut_not_shown | 
| WMLC | switch_incognito_profile | navigate_browser(SiteC) | check_create_shortcut_not_shown | 
| WMLC | navigate_crashed_url | check_create_shortcut_not_shown |
| WMLC | navigate_crashed_url | check_install_icon_not_shown |
| WMLC | navigate_notfound_url | check_create_shortcut_not_shown |
| WMLC | navigate_notfound_url | check_install_icon_not_shown |

## Site promotability checking
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | navigate_browser(SiteA) | check_create_shortcut_shown |
| WMLC | navigate_browser(SiteAFoo) | check_install_icon_shown |
| WMLC | navigate_browser(SiteC) | check_create_shortcut_shown |
| WMLC | navigate_browser(SiteC) | check_install_icon_not_shown |

## In-Browser UX (install icon, launch icon, etc)
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_windowed(SiteAFoo) | navigate_browser(SiteABar) | check_install_icon_shown | 
| WMLC | install_windowed(SiteAFoo) | navigate_browser(SiteABar) | check_launch_icon_not_shown | 
| WMLC | install_windowed | navigate_browser(SiteAFoo) | check_install_icon_not_shown | 
| WMLC | install_windowed | navigate_browser(SiteAFoo) | check_launch_icon_shown | 
| WMLC | install_by_user_windowed | navigate_browser(SiteB) | check_install_icon_shown | 
| WMLC | install_by_user_windowed | navigate_browser(SiteB) | check_launch_icon_not_shown | 
| WMLC | install_by_user_windowed | navigate_pwa_site_a_to(SiteB) | check_app_title_site_a_is(SiteA) | 
| WMLC | install_by_user_windowed | switch_incognito_profile | navigate_browser(SiteA) | check_launch_icon_not_shown |
| WMLC | install_by_user_windowed | set_open_in_tab | check_app_in_list_tabbed | 
| WMLC | install_by_user_windowed | set_open_in_tab | navigate_browser(SiteA) | check_install_icon_shown |
| WMLC | install_by_user_tabbed | set_open_in_window | check_app_in_list_windowed | 
| WMLC | install_by_user_tabbed | set_open_in_window | navigate_browser(SiteA) | check_install_icon_not_shown |
| WMLC | install_by_user_tabbed | set_open_in_window | navigate_browser(SiteA) | check_launch_icon_shown |

## Windows Control Overlay
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| #WMLC | install_windowed(SiteWCO) | check_window_controls_overlay_toggle(SiteWCO, Shown) |
| #WMLC | install_windowed(SiteA) | check_window_controls_overlay_toggle(SiteA, NotShown) |
| #WMLC | install_windowed(SiteWCO) | enable_window_controls_overlay(SiteWCO) | check_window_controls_overlay(SiteWCO, On) | 
| #WMLC | install_windowed(SiteWCO) | enable_window_controls_overlay(SiteWCO) | check_window_controls_overlay_toggle(SiteWCO, Shown) | 
| #WMLC | install_windowed(SiteWCO) | enable_window_controls_overlay(SiteWCO) | disable_window_controls_overlay(SiteWCO) | check_window_controls_overlay(SiteWCO, Off) |
| #WMLC | install_windowed(SiteWCO) | enable_window_controls_overlay(SiteWCO) | disable_window_controls_overlay(SiteWCO) | check_window_controls_overlay_toggle(SiteWCO, Shown) |
| #WMLC | install_windowed(SiteWCO) | enable_window_controls_overlay(SiteWCO) | launch(SiteWCO) | check_window_controls_overlay(SiteWCO, On) |
| #WMLC | install_windowed(SiteB) | manifest_update_display(SiteB, WCO) | await_manifest_update(SiteB) | launch(SiteB) | check_window_controls_overlay_toggle(SiteWCO, Shown) | 
| #WMLC | install_windowed(SiteB) | manifest_update_display(SiteB, WCO) | await_manifest_update(SiteB) | launch(SiteB) | check_window_controls_overlay_toggle(SiteWCO, Shown) | 
| #WMLC | install_windowed(SiteB) | manifest_update_display(SiteB, WCO) | await_manifest_update(SiteB) | launch(SiteB) | enable_window_controls_overlay(SiteB) | check_window_controls_overlay(SiteB, Off) |
| #WMLC | install_windowed(SiteB) | manifest_update_display(SiteB, WCO) | await_manifest_update(SiteB) | launch(SiteB) | enable_window_controls_overlay(SiteB) | check_window_controls_overlay_toggle(SiteB, Shown) |
| #WMLC | install_windowed(SiteB) | manifest_update_display(SiteB, WCO) | await_manifest_update(SiteB) | launch(SiteB) | enable_window_controls_overlay(SiteB) | check_window_controls_overlay_toggle(SiteB, Shown) |
| #WMLC | install_windowed(SiteWCO) | manifest_update_display(SiteWCO, Standalone) | await_manifest_update(SiteWCO) | launch(SiteWCO) | check_window_controls_overlay_toggle(SiteWCO, NotShown) | 
| #WMLC | install_windowed(SiteWCO) | manifest_update_display(SiteWCO, Standalone) | await_manifest_update(SiteWCO) | launch(SiteWCO) | check_window_controls_overlay(SiteWCO, Off) | 

## File Handling

| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install(SiteB) | check_site_handles_file(SiteB, Txt) | check_site_handles_file(SiteB, Png) |
| # Single open & multiple open behavior |
| WMLC | install(SiteB) | launch_file(OneTextFile) | check_file_handling_dialog(Shown) |
| WMLC | install(SiteB) | launch_file(OneTextFile) | file_handling_dialog(Allow, AskAgain) | check_pwa_window_created(SiteB, One) | check_files_loaded_in_site(SiteB, OneTextFile) |
| WMLC | install(SiteB) | launch_file(MultipleTextFiles) | check_file_handling_dialog(Shown)
| WMLC | install(SiteB) | launch_file(MultipleTextFiles) | file_handling_dialog(Allow, AskAgain) | check_pwa_window_created(SiteB, One) | check_files_loaded_in_site(SiteB, MultipleTextFiles) |
| WMLC | install(SiteB) | launch_file(OnePngFile) | check_file_handling_dialog(Shown) |
| WMLC | install(SiteB) | launch_file(OnePngFile) | file_handling_dialog(Allow, AskAgain) | check_pwa_window_created(SiteB, One) | check_files_loaded_in_site(SiteB, OnePngFile) |
| WMLC | install(SiteB) | launch_file(MultiplePngFiles) | check_file_handling_dialog(Shown) |
| WMLC | install(SiteB) | launch_file(MultiplePngFiles) | file_handling_dialog(Allow, AskAgain) | check_pwa_window_created(SiteB, Two) | check_files_loaded_in_site(SiteB, MultiplePngFiles) |
| # Dialog options |
| WMLC | install(SiteB) | launch_file(OneTextFile) | file_handling_dialog(Allow, Remember) | launch_file(OneTextFile) | check_file_handling_dialog(NotShown) | check_pwa_window_created(SiteB, One) | 
| WMLC | install(SiteB) | launch_file(OneTextFile) | file_handling_dialog(Allow, AskAgain) | launch_file(OneTextFile) | check_file_handling_dialog(Shown) |
| WMLC | install(SiteB) | launch_file(OneTextFile) | file_handling_dialog(Deny, AskAgain) | check_window_not_created | check_site_handles_file(SiteB, Txt) | check_site_handles_file(SiteB, Png) | 
| WMLC | install(SiteB) | launch_file(OneTextFile) | file_handling_dialog(Deny, AskAgain) | launch_file(OneTextFile) | check_file_handling_dialog(Shown) |
| WMLC | install(SiteB) | launch_file(OneTextFile) | file_handling_dialog(Deny, Remember) | check_window_not_created | check_site_not_handles_file(SiteB, Txt) | check_site_not_handles_file(SiteB, Png) |
| # Policy approval |
| WMLC | install(SiteB) | add_file_handling_policy_approval(SiteB) | launch_file(OneTextFile) | check_file_handling_dialog(NotShown) | check_pwa_window_created(SiteB, One) | 
| WMLC | install(SiteB) | add_file_handling_policy_approval(SiteB) | remove_file_handling_policy_approval(SiteB) | launch_file(OneTextFile) | check_file_handling_dialog(Shown) |
