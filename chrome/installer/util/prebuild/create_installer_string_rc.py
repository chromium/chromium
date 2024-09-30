#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Defines which strings are to be extracted from Chrome / Chromiums string
# .grd for use in the installer. The variable names MUST not be changed as
# the importing module (base/win/embedded_i18n/create_rc_string.py is
# expecting these names to exist in the module in order to use them.

STRING_IDS = [
  'IDS_ABOUT_VERSION_COMPANY_NAME',
  'IDS_APP_SHORTCUTS_SUBDIR_NAME',
  'IDS_ELEVATION_SERVICE_DESCRIPTION',
  'IDS_INBOUND_MDNS_RULE_DESCRIPTION',
  'IDS_INBOUND_MDNS_RULE_NAME',
  'IDS_INSTALL_EXISTING_VERSION_LAUNCHED',
  'IDS_INSTALL_FAILED',
  'IDS_INSTALL_HIGHER_VERSION',
  'IDS_INSTALL_INSUFFICIENT_RIGHTS',
  'IDS_INSTALL_INVALID_ARCHIVE',
  'IDS_INSTALL_OS_ERROR',
  'IDS_INSTALL_OS_NOT_SUPPORTED',
  'IDS_INSTALL_SINGLETON_ACQUISITION_FAILED',
  'IDS_INSTALL_TEMP_DIR_FAILED',
  'IDS_INSTALL_UNCOMPRESSION_FAILED',
  'IDS_PRODUCT_DESCRIPTION',
  'IDS_PRODUCT_NAME',
  'IDS_SAME_VERSION_REPAIR_FAILED',
  'IDS_SETUP_PATCH_FAILED',
  'IDS_SHORTCUT_NEW_WINDOW',
  'IDS_SHORTCUT_TOOLTIP',
  'IDS_TRACING_SERVICE_DESCRIPTION',
]

# Certain strings are conditional on a brand's install mode (see
# chrome/install_static/install_modes.h for details). This allows
# installer::GetLocalizedString to return a resource specific to the current
# install mode at runtime (e.g., "Google Chrome SxS" as IDS_SHORTCUT_NAME for
# the localized shortcut name for Google Chrome's canary channel).
# l10n_util::GetStringUTF16 (used within the rest of Chrome) is unaffected, and
# will always return the requested string.
#
# Note: Update the test expectations in GetBaseMessageIdForMode.GoogleStringIds
# when adding to/modifying this structure.

MODE_SPECIFIC_STRINGS = {
  'IDS_APP_SHORTCUTS_SUBDIR_NAME': {
    'google_chrome': [
      'IDS_APP_SHORTCUTS_SUBDIR_NAME',
      'IDS_APP_SHORTCUTS_SUBDIR_NAME_BETA',
      'IDS_APP_SHORTCUTS_SUBDIR_NAME_DEV',
      'IDS_APP_SHORTCUTS_SUBDIR_NAME_CANARY',
    ],
    'chromium': [
      'IDS_APP_SHORTCUTS_SUBDIR_NAME',
    ],
  },
  'IDS_INBOUND_MDNS_RULE_DESCRIPTION': {
    'google_chrome': [
      'IDS_INBOUND_MDNS_RULE_DESCRIPTION',
      'IDS_INBOUND_MDNS_RULE_DESCRIPTION_BETA',
      'IDS_INBOUND_MDNS_RULE_DESCRIPTION_DEV',
      'IDS_INBOUND_MDNS_RULE_DESCRIPTION_CANARY',
    ],
    'chromium': [
      'IDS_INBOUND_MDNS_RULE_DESCRIPTION',
    ],
  },
  'IDS_INBOUND_MDNS_RULE_NAME': {
    'google_chrome': [
      'IDS_INBOUND_MDNS_RULE_NAME',
      'IDS_INBOUND_MDNS_RULE_NAME_BETA',
      'IDS_INBOUND_MDNS_RULE_NAME_DEV',
      'IDS_INBOUND_MDNS_RULE_NAME_CANARY',
    ],
    'chromium': [
      'IDS_INBOUND_MDNS_RULE_NAME',
    ],
  },
  # In contrast to the strings above, this one (IDS_PRODUCT_NAME) is used
  # throughout Chrome in mode-independent contexts. Within the installer (the
  # place where this mapping matters), it is only used for mode-specific strings
  # such as the name of Chrome's shortcut.
  'IDS_PRODUCT_NAME': {
    'google_chrome': [
      'IDS_PRODUCT_NAME',
      'IDS_SHORTCUT_NAME_BETA',
      'IDS_SHORTCUT_NAME_DEV',
      'IDS_SXS_SHORTCUT_NAME',
    ],
    'chromium': [
      'IDS_PRODUCT_NAME',
    ],
  },
}
