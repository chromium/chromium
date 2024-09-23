#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
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
        'doc_url': 'https://chromeenterprise.google/policies/',
        'frame_name': 'Chromium Frame',
        'os_name': 'ChromiumOS',
        'webview_name': 'Chromium WebView',
        'win_config': {
            'win': {
                'reg_mandatory_key_name': 'Software\\Policies\\Chromium',
                'reg_recommended_key_name':
                'Software\\Policies\\Chromium\\Recommended',
                'mandatory_category_path': ['chromium'],
                'recommended_category_path': ['chromium_recommended'],
                'category_path_strings': {
                    'chromium': 'Chromium',
                    'chromium_recommended': 'Chromium - {doc_recommended}',
                },
                'namespace': 'Chromium.Policies.Chromium',
            },
            'chrome_os': {
                'reg_mandatory_key_name': 'Software\\Policies\\ChromiumOS',
                'reg_recommended_key_name':
                'Software\\Policies\\ChromiumOS\\Recommended',
                'mandatory_category_path': ['chromium_os'],
                'recommended_category_path': ['chromium_os_recommended'],
                'category_path_strings': {
                    'chromium_os': 'ChromiumOS',
                    'chromium_os_recommended': 'ChromiumOS - {doc_recommended}',
                },
                'namespace': 'Chromium.Policies.ChromiumOS'
            },
        },
        'admx_prefix': 'chromium',
        'linux_policy_path': '/etc/chromium/policies/',
        'bundle_id': 'org.chromium',
    }
  elif '_google_chrome' in defines or '_is_chrome_for_testing_branded' in defines:
    if '_google_chrome' in defines:
      linux_policy_path = '/etc/opt/chrome/policies/'
      win_policy_path = 'Software\\Policies\\Google\\Chrome'
    else:
      linux_policy_path = '/etc/opt/chrome_for_testing/policies/'
      win_policy_path = 'Software\\Policies\\Google\\Chrome for Testing'
    config = {
        'build': 'chrome',
        'app_name': 'Google Chrome',
        'doc_url': 'https://chromeenterprise.google/policies/',
        'frame_name': 'Google Chrome Frame',
        'os_name': 'Google ChromeOS',
        'webview_name': 'Android System WebView',
        'win_config': {
            'win': {
                'reg_mandatory_key_name':
                win_policy_path,
                'reg_recommended_key_name':
                win_policy_path + '\\Recommended',
                'mandatory_category_path':
                ['Google:Cat_Google', 'googlechrome'],
                'recommended_category_path':
                ['Google:Cat_Google', 'googlechrome_recommended'],
                'category_path_strings': {
                    'googlechrome': 'Google Chrome',
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
                'mandatory_category_path':
                ['Google:Cat_Google', 'googlechromeos'],
                'recommended_category_path':
                ['Google:Cat_Google', 'googlechromeos_recommended'],
                'category_path_strings': {
                    'googlechromeos':
                    'Google ChromeOS',
                    'googlechromeos_recommended':
                    'Google ChromeOS - {doc_recommended}'
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
        'linux_policy_path': linux_policy_path,
        'bundle_id': 'com.google.chrome.ios',
    }
  else:
    raise Exception('Unknown build')
  if 'version' in defines:
    config['version'] = defines['version']
  if 'major_version' in defines:
    config['major_version'] = defines['major_version']
  config['win_supported_os'] = 'SUPPORTED_WIN7'
  config['win_supported_os_win7'] = 'SUPPORTED_WIN7_ONLY'
  if 'mac_bundle_id' in defines:
    config['mac_bundle_id'] = defines['mac_bundle_id']
  config['android_webview_restriction_prefix'] = 'com.android.browser:'
  return config
