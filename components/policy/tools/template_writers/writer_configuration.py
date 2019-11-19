#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def GetConfigurationForBuild(defines):
  '''Returns a configuration dictionary for the given build that contains
  build-specific settings and information.

  Args:
    defines: Definitions coming from the build system.

  Raises:
    Exception: If 'defines' contains an unknown build-type.
  '''
  # The prefix of key names in config determines which writer will use their
  # corresponding values:
  #   win: Both ADM and ADMX.
  #   mac: Only plist.
  #   admx: Only ADMX.
  #   adm: Only ADM.
  #   none/other: Used by all the writers.
  # Google:Cat_Google references the external google.admx file.
  # category_path_strings strings in curly braces are looked up from localized
  # 'messages' in policy_templates.json.
  if '_chromium' in defines:
    config = {
        'build': 'chromium',
        'app_name': 'Chromium',
        'frame_name': 'Chromium Frame',
        'os_name': 'Chromium OS',
        'webview_name': 'Chromium WebView',
        'win_config': {
            'win': {
                'reg_mandatory_key_name':
                    'Software\\Policies\\Chromium',
                'reg_recommended_key_name':
                    'Software\\Policies\\Chromium\\Recommended',
                'mandatory_category_path': ['chromium'],
                'recommended_category_path': ['chromium_recommended'],
                'category_path_strings': {
                    'chromium': 'Chromium',
                    'chromium_recommended': 'Chromium - {doc_recommended}',
                },
                'namespace':
                    'Chromium.Policies.Chromium',
            },
            'chrome_os': {
                'reg_mandatory_key_name':
                    'Software\\Policies\\ChromiumOS',
                'reg_recommended_key_name':
                    'Software\\Policies\\ChromiumOS\\Recommended',
                'mandatory_category_path': ['chromium_os'],
                'recommended_category_path': ['chromium_os_recommended'],
                'category_path_strings': {
                    'chromium_os':
                        'Chromium OS',
                    'chromium_os_recommended':
                        'Chromium OS - {doc_recommended}',
                },
                'namespace':
                    'Chromium.Policies.ChromiumOS'
            },
        },
        'admx_prefix': 'chromium',
        'linux_policy_path': '/etc/chromium/policies/',
    }
  elif '_google_chrome' in defines:
    config = {
        'build': 'chrome',
        'app_name': 'Google Chrome',
        'frame_name': 'Google Chrome Frame',
        'os_name': 'Google Chrome OS',
        'webview_name': 'Android System WebView',
        'win_config': {
            'win': {
                'reg_mandatory_key_name':
                    'Software\\Policies\\Google\\Chrome',
                'reg_recommended_key_name':
                    'Software\\Policies\\Google\\Chrome\\Recommended',
                'mandatory_category_path': [
                    'Google:Cat_Google', 'googlechrome'
                ],
                'recommended_category_path': [
                    'Google:Cat_Google', 'googlechrome_recommended'
                ],
                'category_path_strings': {
                    'googlechrome':
                        'Google Chrome',
                    'googlechrome_recommended':
                        'Google Chrome - {doc_recommended}'
                },
                'namespace':
                    'Google.Policies.Chrome',
            },
            'chrome_os': {
                'reg_mandatory_key_name':
                    'Software\\Policies\\Google\\ChromeOS',
                'reg_recommended_key_name':
                    'Software\\Policies\\Google\\ChromeOS\\Recommended',
                'mandatory_category_path': [
                    'Google:Cat_Google', 'googlechromeos'
                ],
                'recommended_category_path': [
                    'Google:Cat_Google', 'googlechromeos_recommended'
                ],
                'category_path_strings': {
                    'googlechromeos':
                        'Google Chrome OS',
                    'googlechromeos_recommended':
                        'Google Chrome OS - {doc_recommended}'
                },
                'namespace':
                    'Google.Policies.ChromeOS',
            },
        },
        # The string 'Google' is defined in google.adml for ADMX, but ADM
        # doesn't support external references, so we define this map here.
        'adm_category_path_strings': {
            'Google:Cat_Google': 'Google'
        },
        'admx_prefix': 'chrome',
        'admx_using_namespaces': {
            'Google': 'Google.Policies'  # prefix: namespace
        },
        'linux_policy_path': '/etc/opt/chrome/policies/',
    }
  else:
    raise Exception('Unknown build')
  if 'version' in defines:
    config['version'] = defines['version']
  config['win_supported_os'] = 'SUPPORTED_WIN7'
  config['win_supported_os_win7'] = 'SUPPORTED_WIN7_ONLY'
  if 'mac_bundle_id' in defines:
    config['mac_bundle_id'] = defines['mac_bundle_id']
  config['android_webview_restriction_prefix'] = 'com.android.browser:'
  return config
