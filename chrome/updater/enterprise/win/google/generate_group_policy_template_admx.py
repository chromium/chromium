#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates a Group Policy admx/adml template file for updater policies.

The resulting strings and files use CRLF as required by gpedit.msc.
"""

from __future__ import print_function

import codecs
import filecmp
import os
import re
import sys

MAIN_POLICY_KEY = r'Software\Policies\Google\Update'

ADMX_HEADER = '<policyDefinitions revision="1.0" schemaVersion="1.0">'

ADMX_ENVIRONMENT = '''
  <policyNamespaces>
    <target namespace="Google.Policies.Update" prefix="update"/>
    <using namespace="Google.Policies" prefix="Google"/>
    <using prefix="windows" namespace="Microsoft.Policies.Windows" />
  </policyNamespaces>
  <supersededAdm fileName="GoogleUpdate.adm" />
  <resources minRequiredRevision="1.0" />
  <supportedOn>
    <definitions>
      <definition name="Sup_GoogleUpdate1_2_145_5"
          displayName="$(string.Sup_GoogleUpdate1_2_145_5)" />
      <definition name="Sup_GoogleUpdate1_3_21_81"
          displayName="$(string.Sup_GoogleUpdate1_3_21_81)" />
      <definition name="Sup_GoogleUpdate1_3_26_0"
          displayName="$(string.Sup_GoogleUpdate1_3_26_0)" />
      <definition name="Sup_GoogleUpdate1_3_33_5"
          displayName="$(string.Sup_GoogleUpdate1_3_33_5)" />
      <definition name="Sup_GoogleUpdate1_3_34_3"
          displayName="$(string.Sup_GoogleUpdate1_3_34_3)" />
      <definition name="Sup_GoogleUpdate1_3_35_441"
          displayName="$(string.Sup_GoogleUpdate1_3_35_441)" />
      <definition name="Sup_GoogleUpdate1_3_35_453"
          displayName="$(string.Sup_GoogleUpdate1_3_35_453)" />
    </definitions>
  </supportedOn>
'''

ADMX_CATEGORIES = r'''
  <categories>
    <category name="Cat_GoogleUpdate" displayName="$(string.Cat_GoogleUpdate)"
        explainText="$(string.Explain_GoogleUpdate)">
      <parentCategory ref="Google:Cat_Google" />
    </category>
    <category name="Cat_Preferences" displayName="$(string.Cat_Preferences)"
        explainText="$(string.Explain_Preferences)">
      <parentCategory ref="Cat_GoogleUpdate" />
    </category>
    <category name="Cat_ProxyServer" displayName="$(string.Cat_ProxyServer)">
      <parentCategory ref="Cat_GoogleUpdate" />
    </category>
    <category name="Cat_Applications" displayName="$(string.Cat_Applications)"
        explainText="$(string.Explain_Applications)">
      <parentCategory ref="Cat_GoogleUpdate" />
    </category>
%(AppCategorList)s
  </categories>
'''

ADMX_POLICIES = r'''
  <policies>
    <policy name="Pol_AutoUpdateCheckPeriod" class="Machine"
        displayName="$(string.Pol_AutoUpdateCheckPeriod)"
        explainText="$(string.Explain_AutoUpdateCheckPeriod)"
        presentation="$(presentation.Pol_AutoUpdateCheckPeriod)"
        key="%(RootPolicyKey)s">
      <parentCategory ref="Cat_Preferences" />
      <supportedOn ref="Sup_GoogleUpdate1_2_145_5" />
      <elements>
        <decimal id="Part_AutoUpdateCheckPeriod"
            key="%(RootPolicyKey)s"
            valueName="AutoUpdateCheckPeriodMinutes"
            required="true" minValue="0" maxValue="43200" />
      </elements>
    </policy>
    <policy name="Pol_DownloadPreference" class="Machine"
        displayName="$(string.Pol_DownloadPreference)"
        explainText="$(string.Explain_DownloadPreference)"
        presentation="$(presentation.Pol_DownloadPreference)"
        key="%(RootPolicyKey)s">
      <parentCategory ref="Cat_Preferences" />
      <supportedOn ref="Sup_GoogleUpdate1_3_26_0" />
      <elements>
        <enum id="Part_DownloadPreference" key="%(RootPolicyKey)s"
            valueName="DownloadPreference">
          <item displayName="$(string.DownloadPreference_DropDown)">
            <value>
              <string>cacheable</string>
            </value>
          </item>
        </enum>
      </elements>
    </policy>
    <policy name="Pol_UpdateCheckSuppressedPeriod" class="Machine"
        displayName="$(string.Pol_UpdateCheckSuppressedPeriod)"
        explainText="$(string.Explain_UpdateCheckSuppressedPeriod)"
        presentation="$(presentation.Pol_UpdateCheckSuppressedPeriod)"
        key="Software\Policies\Google\Update">
      <parentCategory ref="Cat_Preferences" />
      <supportedOn ref="Sup_GoogleUpdate1_3_33_5" />
      <elements>
        <decimal id="Part_UpdateCheckSuppressedStartHour"
            key="Software\Policies\Google\Update"
            valueName="UpdatesSuppressedStartHour"
            required="true" minValue="0" maxValue="23" />
        <decimal id="Part_UpdateCheckSuppressedStartMin"
            key="Software\Policies\Google\Update"
            valueName="UpdatesSuppressedStartMin"
            required="true" minValue="0" maxValue="59" />
        <decimal id="Part_UpdateCheckSuppressedDurationMin"
            key="Software\Policies\Google\Update"
            valueName="UpdatesSuppressedDurationMin"
            required="true" minValue="1" maxValue="960" />
      </elements>
    </policy>
    <policy name="Pol_CloudPolicyOverridesPlatformPolicy" class="Machine"
        displayName="$(string.Pol_CloudPolicyOverridesPlatformPolicy)"
        explainText="$(string.Explain_CloudPolicyOverridesPlatformPolicy)"
        presentation="$(presentation.Pol_CloudPolicyOverridesPlatformPolicy)"
        key="%(RootPolicyKey)s" valueName="CloudPolicyOverridesPlatformPolicy">
      <parentCategory ref="Cat_Preferences" />
      <supportedOn ref="Sup_GoogleUpdate1_3_35_441" />
      <enabledValue><decimal value="1" /></enabledValue>
      <disabledValue><decimal value="0" /></disabledValue>
    </policy>
    <policy name="Pol_ProxyMode" class="Machine"
        displayName="$(string.Pol_ProxyMode)"
        explainText="$(string.Explain_ProxyMode)"
        presentation="$(presentation.Pol_ProxyMode)"
        key="%(RootPolicyKey)s">
      <parentCategory ref="Cat_ProxyServer" />
      <supportedOn ref="Sup_GoogleUpdate1_3_21_81" />
      <elements>
        <enum id="Part_ProxyMode" key="%(RootPolicyKey)s"
            valueName="ProxyMode">
          <item displayName="$(string.ProxyDisabled_DropDown)">
            <value>
              <string>direct</string>
            </value>
          </item>
          <item displayName="$(string.ProxyAutoDetect_DropDown)">
            <value>
              <string>auto_detect</string>
            </value>
          </item>
          <item displayName="$(string.ProxyPacScript_DropDown)">
            <value>
              <string>pac_script</string>
            </value>
          </item>
          <item displayName="$(string.ProxyFixedServers_DropDown)">
            <value>
              <string>fixed_servers</string>
            </value>
          </item>
          <item displayName="$(string.ProxyUseSystem_DropDown)">
            <value>
              <string>system</string>
            </value>
          </item>
        </enum>
      </elements>
    </policy>
    <policy name="Pol_ProxyServer" class="Machine"
        displayName="$(string.Pol_ProxyServer)"
        explainText="$(string.Explain_ProxyServer)"
        presentation="$(presentation.Pol_ProxyServer)"
        key="%(RootPolicyKey)s">
      <parentCategory ref="Cat_ProxyServer" />
      <supportedOn ref="Sup_GoogleUpdate1_3_21_81" />
      <elements>
        <text id="Part_ProxyServer" valueName="ProxyServer" />
      </elements>
    </policy>
    <policy name="Pol_ProxyPacUrl" class="Machine"
        displayName="$(string.Pol_ProxyPacUrl)"
        explainText="$(string.Explain_ProxyPacUrl)"
        presentation="$(presentation.Pol_ProxyPacUrl)"
        key="%(RootPolicyKey)s">
      <parentCategory ref="Cat_ProxyServer" />
      <supportedOn ref="Sup_GoogleUpdate1_3_21_81" />
      <elements>
        <text id="Part_ProxyPacUrl" valueName="ProxyPacUrl" />
      </elements>
    </policy>

    <policy name="Pol_DefaultAllowInstallation" class="Machine"
        displayName="$(string.Pol_DefaultAllowInstallation)"
        explainText="$(string.Explain_DefaultAllowInstallation)"
        presentation="$(presentation.Pol_DefaultAllowInstallation)"
        key="%(RootPolicyKey)s">
      <parentCategory ref="Cat_Applications" />
      <supportedOn ref="Sup_GoogleUpdate1_2_145_5" />
      <elements>
        <enum id="Part_InstallPolicy" key="%(RootPolicyKey)s"
            valueName="InstallDefault" required="true">
          <item displayName="$(string.Name_InstallsEnabled)">
            <value>
              <decimal value="1" />
            </value>
          </item>
          <item displayName="$(string.Name_InstallsEnabledMachineOnly)">
            <value>
              <decimal value="4" />
            </value>
          </item>
          <item displayName="$(string.Name_InstallsDisabled)">
            <value>
              <decimal value="0" />
            </value>
          </item>
        </enum>
      </elements>
    </policy>
    <policy name="Pol_DefaultUpdatePolicy" class="Machine"
        displayName="$(string.Pol_DefaultUpdatePolicy)"
        explainText="$(string.Explain_DefaultUpdatePolicy)"
        presentation="$(presentation.Pol_DefaultUpdatePolicy)"
        key="%(RootPolicyKey)s">
      <parentCategory ref="Cat_Applications" />
      <supportedOn ref="Sup_GoogleUpdate1_2_145_5" />
      <elements>
        <enum id="Part_UpdatePolicy" key="%(RootPolicyKey)s"
            valueName="UpdateDefault" required="true">
          <item displayName="$(string.Name_UpdatesEnabled)">
            <value>
              <decimal value="1" />
            </value>
          </item>
          <item displayName="$(string.Name_ManualUpdatesOnly)">
            <value>
              <decimal value="2" />
            </value>
          </item>
          <item displayName="$(string.Name_AutomaticUpdatesOnly)">
            <value>
              <decimal value="3" />
            </value>
          </item>
          <item displayName="$(string.Name_UpdatesDisabled)">
            <value>
              <decimal value="0" />
            </value>
          </item>
        </enum>
      </elements>
    </policy>
%(AppPolicyList)s
  </policies>
'''

INSTALL_POLICY_FORCE_INSTALL_MACHINE = r'''
          <item displayName="$(string.Name_ForceInstallsMachine)">
            <value>
              <decimal value="5" />
            </value>
          </item>'''

INSTALL_POLICY_FORCE_INSTALL_USER = r'''
          <item displayName="$(string.Name_ForceInstallsUser)">
            <value>
              <decimal value="6" />
            </value>
          </item>'''

ADMX_APP_POLICY_TEMPLATE = '''\
    <policy name="Pol_AllowInstallation%(AppLegalId)s" class="Machine"
        displayName="$(string.Pol_AllowInstallation)"
        explainText="$(string.Explain_Install%(AppLegalId)s)"
        presentation="$(presentation.Pol_AllowInstallation)"
        key="%(RootPolicyKey)s">
      <parentCategory ref="Cat_%(AppLegalId)s" />
      <supportedOn ref="Sup_GoogleUpdate1_2_145_5" />
      <elements>
        <enum id="Part_InstallPolicy"
             valueName="Install%(AppGuid)s" required="true">
          <item displayName="$(string.Name_InstallsEnabled)">
            <value>
              <decimal value="1" />
            </value>
          </item>
          <item displayName="$(string.Name_InstallsEnabledMachineOnly)">
            <value>
              <decimal value="4" />
            </value>
          </item>
          <item displayName="$(string.Name_InstallsDisabled)">
            <value>
              <decimal value="0" />
            </value>
          </item>%(ForceInstalls)s
        </enum>
      </elements>
    </policy>
    <policy name="Pol_UpdatePolicy%(AppLegalId)s" class="Machine"
        displayName="$(string.Pol_UpdatePolicy)"
        explainText="$(string.Explain_AutoUpdate%(AppLegalId)s)"
        presentation="$(presentation.Pol_UpdatePolicy)"
        key="%(RootPolicyKey)s">
      <parentCategory ref="Cat_%(AppLegalId)s" />
      <supportedOn ref="Sup_GoogleUpdate1_2_145_5" />
      <elements>
        <enum id="Part_UpdatePolicy"
             valueName="Update%(AppGuid)s" required="true">
          <item displayName="$(string.Name_UpdatesEnabled)">
            <value>
              <decimal value="1" />
            </value>
          </item>
          <item displayName="$(string.Name_ManualUpdatesOnly)">
            <value>
              <decimal value="2" />
            </value>
          </item>
          <item displayName="$(string.Name_AutomaticUpdatesOnly)">
            <value>
              <decimal value="3" />
            </value>
          </item>
          <item displayName="$(string.Name_UpdatesDisabled)">
            <value>
              <decimal value="0" />
            </value>
          </item>
        </enum>
      </elements>
    </policy>
    <policy name="Pol_TargetChannel%(AppLegalId)s" class="Machine"
        displayName="$(string.Pol_TargetChannel)"
        explainText="$(string.Explain_TargetChannel%(AppLegalId)s)"
        presentation="$(presentation.Pol_TargetChannel)"
        key="%(RootPolicyKey)s">
      <parentCategory ref="Cat_%(AppLegalId)s" />
      <supportedOn ref="Sup_GoogleUpdate1_3_35_453" />
      <elements>
        <text id="Part_TargetChannel"
            valueName="TargetChannel%(AppGuid)s" />
      </elements>
    </policy>
    <policy name="Pol_TargetVersionPrefix%(AppLegalId)s" class="Machine"
        displayName="$(string.Pol_TargetVersionPrefix)"
        explainText="$(string.Explain_TargetVersionPrefix%(AppLegalId)s)"
        presentation="$(presentation.Pol_TargetVersionPrefix)"
        key="%(RootPolicyKey)s">
      <parentCategory ref="Cat_%(AppLegalId)s" />
      <supportedOn ref="Sup_GoogleUpdate1_3_33_5" />
      <elements>
        <text id="Part_TargetVersionPrefix"
            valueName="TargetVersionPrefix%(AppGuid)s" />
      </elements>
    </policy>
    <policy name="Pol_RollbackToTargetVersion%(AppLegalId)s" class="Machine"
        displayName="$(string.Pol_RollbackToTargetVersion)"
        explainText="$(string.Explain_RollbackToTargetVersion%(AppLegalId)s)"
        presentation="$(presentation.Pol_RollbackToTargetVersion)"
        key="%(RootPolicyKey)s"
        valueName="RollbackToTargetVersion%(AppGuid)s">
      <parentCategory ref="Cat_%(AppLegalId)s" />
      <supportedOn ref="Sup_GoogleUpdate1_3_34_3" />
      <enabledValue><decimal value="1" /></enabledValue>
      <disabledValue><decimal value="0" /></disabledValue>
    </policy>'''

ADMX_FOOTER = '</policyDefinitions>'


def _CreateLegalIdentifier(input_string):
    """Converts input_string to a legal identifier for ADMX/ADML files.

  Changes some characters that do not necessarily cause problems and may not
  handle all cases.

  Args:
    input_string: Text to convert to a legal identifier.

  Returns:
    String containing a legal identifier based on input_string.
  """
    return re.sub(r'[\W_]', '', input_string)


def GenerateGroupPolicyTemplateAdmx(apps):
    """Generates a Group Policy template (ADMX format)for the specified apps.

  Replaces LF in strings above with CRLF as required by gpedit.msc.
  When writing the resulting contents to a file, use binary mode to ensure the
  CRLFs are preserved.

  Args:
    apps: A list of tuples containing information about each app.
        Each element of the list is a tuple of:
          * app name
          * app ID
          * optional string to append to the auto-update explanation
            - Should start with a space or double new line.

  Returns:
    String containing the contents of the .ADMX file.
  """

    def _GenerateCategories(apps):
        """Generates category string for each of the specified apps.

    Args:
      apps: list of tuples containing information about the apps.

    Returns:
      String containing concatenated copies of the category string for each app
      in apps, each populated with the appropriate app-specific strings.
    """

        admx_app_category_template = (
            '    <category name="Cat_%(AppLegalId)s"\n'
            '        displayName="$(string.Cat_%(AppLegalId)s)">\n'
            '      <parentCategory ref="Cat_Applications" />\n'
            '    </category>')

        app_category_list = []
        for app in apps:
            app_name = app[0]
            app_category_list.append(
                admx_app_category_template %
                {'AppLegalId': _CreateLegalIdentifier(app_name)})

        return ADMX_CATEGORIES % {
            'AppCategorList': '\n'.join(app_category_list)
        }

    def _GeneratePolicies(apps):
        """Generates policy string for each of the specified apps.

    Args:
      apps: list of tuples containing information about the apps.

    Returns:
      String containing concatenated copies of the policy template for each app
      in apps, each populated with the appropriate app-specific strings.
    """

        app_policy_list = []
        for app_name, app_guid, _, _, force_install_machine, force_install_user\
        in apps:
            force_installs = ''
            if force_install_machine:
                force_installs += INSTALL_POLICY_FORCE_INSTALL_MACHINE
            if force_install_user:
                force_installs += INSTALL_POLICY_FORCE_INSTALL_USER

            app_policy_list.append(
                ADMX_APP_POLICY_TEMPLATE % {
                    'AppLegalId': _CreateLegalIdentifier(app_name),
                    'AppGuid': app_guid,
                    'RootPolicyKey': MAIN_POLICY_KEY,
                    'ForceInstalls': force_installs,
                })

        return ADMX_POLICIES % {
            'AppPolicyList': '\n'.join(app_policy_list),
            'RootPolicyKey': MAIN_POLICY_KEY,
        }

    target_contents = [
        ADMX_HEADER,
        ADMX_ENVIRONMENT,
        _GenerateCategories(apps),
        _GeneratePolicies(apps),
        ADMX_FOOTER,
    ]

    return ''.join(target_contents)


ADML_HEADER = '''\
<policyDefinitionResources revision="1.0" schemaVersion="1.0">
'''

ADML_ENVIRONMENT = '''\
  <displayName>
  </displayName>
  <description>
  </description>
'''

ADML_DEFAULT_ROLLBACK_DISCLAIMER = (
    'This policy is meant to serve as temporary measure when Enterprise '
    'Administrators need to downgrade for business reasons. To ensure '
    'users are protected by the latest security updates, the most recent '
    'version should be used. When versions are downgraded to older '
    'versions, there could be incompatibilities.')

FORCE_INSTALLS_MACHINE_EXPLAIN = (
    'Force Installs (Machine-Wide): Allows Deploying %s to all machines where '
    'Google Update is pre-installed. Requires Google Update 1.3.36.82 or higher'
    '.\n\n')
FORCE_INSTALLS_USER_EXPLAIN = (
    'Force Installs (Per-User): Allows Deploying %s on a Per-User basis to all '
    'machines where Google Update is pre-installed Per-User. Requires Google '
    'Update 1.3.36.82 or higher.\n\n')

ADML_DOMAIN_REQUIREMENT_EN = (
    'This policy is available only on Windows instances that are joined to a '
    'Microsoft&#x00AE; Active Directory&#x00AE; domain.')

ADML_PREDEFINED_STRINGS_TABLE_EN = [
    ('Sup_GoogleUpdate1_2_145_5', 'At least Google Update 1.2.145.5'),
    ('Sup_GoogleUpdate1_3_21_81', 'At least Google Update 1.3.21.81'),
    ('Sup_GoogleUpdate1_3_26_0', 'At least Google Update 1.3.26.0'),
    ('Sup_GoogleUpdate1_3_33_5', 'At least Google Update 1.3.33.5'),
    ('Sup_GoogleUpdate1_3_34_3', 'At least Google Update 1.3.34.3'),
    ('Sup_GoogleUpdate1_3_35_441', 'At least Google Update 1.3.35.441'),
    ('Sup_GoogleUpdate1_3_35_453', 'At least Google Update 1.3.35.453'),
    ('Cat_GoogleUpdate', 'Google Update'),
    ('Cat_Preferences', 'Preferences'),
    ('Cat_ProxyServer', 'Proxy Server'),
    ('Cat_Applications', 'Applications'),
    ('Pol_AutoUpdateCheckPeriod', 'Auto-update check period override'),
    ('Pol_UpdateCheckSuppressedPeriod',
     'Time period in each day to suppress auto-update check'),
    ('Pol_CloudPolicyOverridesPlatformPolicy', 'Cloud Policy takes precedence '
     'over Group Policy'),
    ('Pol_DownloadPreference', 'Download URL class override'),
    ('DownloadPreference_DropDown', 'Cacheable download URLs'),
    ('Pol_ProxyMode', 'Choose how to specify proxy server settings'),
    ('Pol_ProxyServer', 'Address or URL of proxy server'),
    ('Pol_ProxyPacUrl', 'URL to a proxy .pac file'),
    ('Pol_DefaultAllowInstallation', 'Allow installation default'),
    ('Pol_AllowInstallation', 'Allow installation'),
    ('Pol_DefaultUpdatePolicy', 'Update policy override default'),
    ('Pol_UpdatePolicy', 'Update policy override'),
    ('Pol_TargetChannel', 'Target Channel override'),
    ('Pol_TargetVersionPrefix', 'Target version prefix override'),
    ('Pol_RollbackToTargetVersion', 'Rollback to Target version'),
    ('Part_AutoUpdateCheckPeriod', 'Minutes between update checks'),
    ('Part_UpdateCheckSuppressedStartHour',
     'Hour in a day that start to suppress update check'),
    ('Part_UpdateCheckSuppressedStartMin',
     'Minute in hour that starts to suppress update check'),
    ('Part_UpdateCheckSuppressedDurationMin',
     'Number of minutes to suppress update check each day'),
    ('Part_ProxyMode', 'Choose how to specify proxy server settings'),
    ('Part_ProxyServer', 'Address or URL of proxy server'),
    ('Part_ProxyPacUrl', 'URL to a proxy .pac file'),
    ('Part_InstallPolicy', 'Policy'),
    ('Name_InstallsEnabled', 'Always allow Installs (recommended)'),
    ('Name_InstallsEnabledMachineOnly',
     'Always allow Machine-Wide Installs, but not Per-User Installs.'),
    ('Name_InstallsDisabled', 'Installs disabled'),
    ('Name_ForceInstallsMachine', 'Force Installs (Machine-Wide)'),
    ('Name_ForceInstallsUser', 'Force Installs (Per-User)'),
    ('Part_UpdatePolicy', 'Policy'),
    ('Part_TargetChannel', 'Target Channel'),
    ('Part_TargetVersionPrefix', 'Target version prefix'),
    ('Name_UpdatesEnabled', 'Always allow updates (recommended)'),
    ('Name_ManualUpdatesOnly', 'Manual updates only'),
    ('Name_AutomaticUpdatesOnly', 'Automatic silent updates only'),
    ('Name_UpdatesDisabled', 'Updates disabled'),
    ('ProxyDisabled_DropDown', 'Never use a proxy'),
    ('ProxyAutoDetect_DropDown', 'Auto detect proxy settings'),
    ('ProxyPacScript_DropDown', 'Use a .pac proxy script'),
    ('ProxyFixedServers_DropDown', 'Use fixed proxy servers'),
    ('ProxyUseSystem_DropDown', 'Use system proxy settings'),
    ('Explain_GoogleUpdate',
     'Policies to control the installation and updating of Google applications '
     'that use Google Update/Google Installer.'),
    ('Explain_Preferences', 'General policies for Google Update.'),
    ('Explain_AutoUpdateCheckPeriod',
     'Minimum number of minutes between automatic update checks.\n\n'
     'Set this policy to the value 0 to disable all periodic network traffic '
     'by Google Update. This is not recommended, as it prevents Google Update '
     'itself from receiving stability and security updates.\n\nThe "Update '
     'policy override default" and per-application "Update policy override" '
     'settings should be used to manage application updates rather than this '
     'setting.\n\n'
     '%s' % ADML_DOMAIN_REQUIREMENT_EN),
    ('Explain_DownloadPreference',
     'If enabled, the Google Update server will attempt to provide '
     'cache-friendly URLs for update payloads in its responses.\n\n'
     '%s' % ADML_DOMAIN_REQUIREMENT_EN),
    ('Explain_UpdateCheckSuppressedPeriod',
     'If this setting is enabled, update checks will be suppressed during '
     'each day starting from Hour:Minute for a period of Duration (in minutes).'
     ' Duration does not account for daylight savings time. So for instance, '
     'if the start time is 22:00, and with a duration of 480 minutes, the '
     'updates will be suppressed for 8 hours regardless of whether daylight '
     'savings time changes happen in between.\n\n'
     '%s' % ADML_DOMAIN_REQUIREMENT_EN),
    ('Explain_CloudPolicyOverridesPlatformPolicy',
     'If the policy is Enabled, Cloud Policy settings take precedence over '
     'Group Policy settings for Google Update.\n\n'
     'If this policy is Not Configured or not Enabled, Group Policy takes '
     'precedence over Cloud Policy.\n\n'
     'This policy is only available as a mandatory machine platform policy and '
     'it only affects machine scope cloud policies.\n\n'
     '%s' % ADML_DOMAIN_REQUIREMENT_EN),
    ('Explain_ProxyMode',
     'Allows you to specify the proxy server used by Google Update.\n\n'
     'If you choose to never use a proxy server and always connect directly, '
     'all other options are ignored.\n\n'
     'If you choose to use system proxy settings or auto detect the proxy '
     'server, all other options are ignored.\n\n'
     'If you choose fixed server proxy mode, you can specify further options '
     'in \'Address or URL of proxy server\'.\n\n'
     'If you choose to use a .pac proxy script, you must specify the URL to '
     'the script in \'URL to a proxy .pac file\'.\n\n'
     '%s' % ADML_DOMAIN_REQUIREMENT_EN),
    ('Explain_ProxyServer',
     'You can specify the URL of the proxy server here.\n\n'
     'This policy only takes effect if you have selected manual proxy settings '
     'at \'Choose how to specify proxy server settings\'.\n\n'
     '%s' % ADML_DOMAIN_REQUIREMENT_EN),
    ('Explain_ProxyPacUrl',
     'You can specify a URL to a proxy .pac file here.\n\n'
     'This policy only takes effect if you have selected manual proxy settings '
     'at \'Choose how to specify proxy server settings\'.\n\n'
     '%s' % ADML_DOMAIN_REQUIREMENT_EN),
    ('Explain_Applications', 'Policies for individual applications.\n\n'
     'An updated ADMX/ADML template will be required to support '
     'Google applications released in the future.'),
    ('Explain_DefaultAllowInstallation',
     'Specifies the default behavior for whether Google software can be '
     'installed using Google Update/Google Installer.\n\n'
     'Can be overridden by the "Allow installation" for individual '
     'applications.\n\n'
     'Only affects installation of Google software using Google Update/Google '
     'Installer. Cannot prevent running the application installer directly or '
     'installation of Google software that does not use Google Update/Google '
     'Installer for installation.\n\n'
     '%s' % ADML_DOMAIN_REQUIREMENT_EN),
    ('Explain_DefaultUpdatePolicy',
     'Specifies the default policy for software updates from Google.\n\n'
     'Can be overridden by the "Update policy override" for individual '
     'applications.\n\n'
     'Options:\n'
     ' - Always allow updates: Updates are always applied when found, either '
     'by periodic update check or by a manual update check.\n'
     ' - Manual updates only: Updates are only applied when the user does a '
     'manual update check. (Not all apps provide an interface for this.)\n'
     ' - Automatic silent updates only: Updates are only applied when they are '
     'found via the periodic update check.\n'
     ' - Updates disabled: Never apply updates.\n\n'
     'If you select manual updates, you should periodically check for updates '
     'using each application\'s manual update mechanism if available. If you '
     'disable updates, you should periodically check for updates and '
     'distribute them to users.\n\n'
     'Only affects updates for Google software that uses Google Update for '
     'updates. Does not prevent auto-updates of Google software that does not '
     'use Google Update for updates.\n\n'
     'Updates for Google Update are not affected by this setting; Google '
     'Update will continue to update itself while it is installed.\n\n'
     'WARNING: Disabing updates will also prevent updates of any new Google '
     'applications released in the future, possibly including dependencies for '
     'future versions of installed applications.\n\n'
     '%s' % ADML_DOMAIN_REQUIREMENT_EN),
]

ADML_PRESENTATIONS = '''\
      <presentation id="Pol_AutoUpdateCheckPeriod">
        <decimalTextBox refId="Part_AutoUpdateCheckPeriod" defaultValue="295"
            spinStep="60">Minutes between update checks</decimalTextBox>
      </presentation>
      <presentation id="Pol_UpdateCheckSuppressedPeriod">
        <decimalTextBox refId="Part_UpdateCheckSuppressedStartHour"
            defaultValue="0" spinStep="1">Hour</decimalTextBox>
        <decimalTextBox refId="Part_UpdateCheckSuppressedStartMin"
            defaultValue="0" spinStep="1">Minute</decimalTextBox>
        <decimalTextBox refId="Part_UpdateCheckSuppressedDurationMin"
            defaultValue="60">Duration</decimalTextBox>
      </presentation>
      <presentation id="Pol_CloudPolicyOverridesPlatformPolicy" />
      <presentation id="Pol_DownloadPreference">
        <dropdownList refId="Part_DownloadPreference"
            defaultItem="0">Type of download URL to request</dropdownList>
      </presentation>
      <presentation id="Pol_ProxyMode">
        <dropdownList refId="Part_ProxyMode"
            defaultItem="0">Choose how to specify proxy server settings
        </dropdownList>
      </presentation>
      <presentation id="Pol_ProxyServer">
        <textBox refId="Part_ProxyServer">
          <label>Address or URL of proxy server</label>
          <defaultValue></defaultValue>
        </textBox>
      </presentation>
      <presentation id="Pol_ProxyPacUrl">
        <textBox refId="Part_ProxyPacUrl">
          <label>URL to a proxy .pac file</label>
          <defaultValue></defaultValue>
        </textBox>
      </presentation>
      <presentation id="Pol_DefaultAllowInstallation">
        <dropdownList refId="Part_InstallPolicy"
            defaultItem="0">Policy</dropdownList>
      </presentation>
      <presentation id="Pol_DefaultUpdatePolicy">
        <dropdownList refId="Part_UpdatePolicy"
            defaultItem="0">Policy</dropdownList>
      </presentation>
      <presentation id="Pol_AllowInstallation">
        <dropdownList refId="Part_InstallPolicy"
            defaultItem="0">Policy</dropdownList>
      </presentation>
      <presentation id="Pol_UpdatePolicy">
        <dropdownList refId="Part_UpdatePolicy"
            defaultItem="0">Policy</dropdownList>
      </presentation>
      <presentation id="Pol_TargetChannel">
        <textBox refId="Part_TargetChannel">
          <label>Target Channel</label>
          <defaultValue></defaultValue>
        </textBox>
      </presentation>
      <presentation id="Pol_TargetVersionPrefix">
        <textBox refId="Part_TargetVersionPrefix">
          <label>Target version prefix</label>
          <defaultValue></defaultValue>
        </textBox>
      </presentation>
      <presentation id="Pol_RollbackToTargetVersion" />
'''

ADML_RESOURCE_TABLE_TEMPLATE = '''
  <resources>
    <stringTable>
%s
    </stringTable>
    <presentationTable>
%s
    </presentationTable>
  </resources>
'''

ADML_FOOTER = '</policyDefinitionResources>'


def GenerateGroupPolicyTemplateAdml(apps):
    """Generates a Group Policy template (ADML format)for the specified apps.

  Replaces LF in strings above with CRLF as required by gpedit.msc.
  When writing the resulting contents to a file, use binary mode to ensure the
  CRLFs are preserved.

  Args:
    apps: A list of tuples containing information about each app.
        Each element of the list is a tuple of:
          * app name
          * app ID
          * optional string to append to the auto-update explanation
            - Should start with a space or double new line.

  Returns:
    String containing the contents of the .ADML file.
  """

    string_definition_list = ADML_PREDEFINED_STRINGS_TABLE_EN[:]
    for app in apps:
        app_name = app[0]
        app_legal_id = _CreateLegalIdentifier(app_name)
        app_additional_help_msg = app[2]
        rollback_disclaimer = app[3]
        if not rollback_disclaimer:
            rollback_disclaimer = ADML_DEFAULT_ROLLBACK_DISCLAIMER

        force_install_machine = app[4]
        force_install_user = app[5]
        force_installs_explain = ''
        if force_install_machine:
            force_installs_explain += FORCE_INSTALLS_MACHINE_EXPLAIN % app_name
        if force_install_user:
            force_installs_explain += FORCE_INSTALLS_USER_EXPLAIN % app_name

        app_category = ('Cat_' + app_legal_id, app_name)
        string_definition_list.append(app_category)

        app_install_policy_explanation = (
            'Explain_Install' + app_legal_id,
            'Specifies whether %s can be installed using Google Update/Google '
            'Installer.\n\n'
            'If this policy is not configured, %s can be installed as specified'
            ' by "Allow installation default".\n\n'
            '%s'
            '%s' % (app_name, app_name, force_installs_explain,
                    ADML_DOMAIN_REQUIREMENT_EN))

        string_definition_list.append(app_install_policy_explanation)

        app_auto_update_policy_explanation = (
            'Explain_AutoUpdate' + app_legal_id,
            'Specifies how Google Update handles available %s updates '
            'from Google.\n\n'
            'If this policy is not configured, Google Update handles available '
            'updates as specified by "Update policy override default".\n\n'
            'Options:\n'
            ' - Always allow updates: Updates are always applied when found, '
            'either by periodic update check or by a manual update check.\n'
            ' - Manual updates only: Updates are only applied when the user '
            'does a manual update check. (Not all apps provide an interface '
            ' for this.)\n'
            ' - Automatic silent updates only: Updates are only applied when '
            'they are found via the periodic update check.\n'
            ' - Updates disabled: Never apply updates.\n\n'
            'If you select manual updates, you should periodically check for '
            'updates using the application\'s manual update mechanism if '
            'available. If you disable updates, you should periodically check '
            'for updates and distribute them to users.%s\n\n'
            '%s' %
            (app_name, app_additional_help_msg, ADML_DOMAIN_REQUIREMENT_EN))
        string_definition_list.append(app_auto_update_policy_explanation)

        app_target_channel_explanation = (
            'Explain_TargetChannel' + app_legal_id,
            'Specifies which Channel %s should be updated to.\n\n'
            'When this policy is enabled, the app will be updated to the '
            'Channel with this policy value.\n\nSome examples:\n'
            '1) Not configured: app will be updated to the latest version '
            'available in the default Channel for the app.\n'
            '2) Policy value is set to "stable": the app will be updated to the'
            ' latest stable version.\n'
            '2) Policy value is set to "beta": the app will be updated to the '
            'latest beta version.\n'
            '2) Policy value is set to "dev": the app will be updated to the '
            'latest dev version.\n'
            '%s' % (app_name, ADML_DOMAIN_REQUIREMENT_EN))
        string_definition_list.append(app_target_channel_explanation)

        app_target_version_prefix_explanation = (
            'Explain_TargetVersionPrefix' + app_legal_id,
            'Specifies which version %s should be updated to.\n\n'
            'When this policy is enabled, the app will be updated to the '
            'version prefixed with this policy value.\n\nSome examples:\n'
            '1) Not configured: app will be updated to the latest version '
            'available.\n'
            '2) Policy value is set to "55.": the app will be updated to any '
            'minor version of 55 (e.g., 55.24.34 or 55.60.2).\n'
            '3) Policy value is "55.2.": the app will be updated to any minor '
            'version of 55.2 (e.g., 55.2.34 or 55.2.2).\n'
            '4) Policy value is "55.24.34": the app will be updated to this '
            'specific version only.\n\n'
            '%s' % (app_name, ADML_DOMAIN_REQUIREMENT_EN))
        string_definition_list.append(app_target_version_prefix_explanation)

        app_rollback_to_target_version_explanation = (
            'Explain_RollbackToTargetVersion' + app_legal_id,
            'Specifies that Google Update should roll installations of %s back '
            'if the client has a higher version than that available.\n\n'
            'If this policy is not configured or is disabled, installs that '
            'have a version higher than that available will be left as-is. This'
            ' could be the case if "Target channel override" is set to a '
            'Channel with a lower version, if "Target version prefix override" '
            'matches a lower version on the Channel, or if a user had '
            'installed a higher version.\n\n'
            'If this policy is enabled, installs that have a version higher '
            'than that available will be downgraded to the highest available '
            'version, respecting any configured target Channel and target '
            'version.\n\n'
            '%s\n\n'
            '%s' % (app_name, rollback_disclaimer, ADML_DOMAIN_REQUIREMENT_EN))
        string_definition_list.append(
            app_rollback_to_target_version_explanation)

    app_resource_strings = []
    for entry in string_definition_list:
        app_resource_strings.append('      <string id="%s">%s</string>' %
                                    (entry[0], entry[1]))

    app_resource_tables = (
        ADML_RESOURCE_TABLE_TEMPLATE %
        ('\n'.join(app_resource_strings), ADML_PRESENTATIONS))

    target_contents = [
        ADML_HEADER,
        ADML_ENVIRONMENT,
        app_resource_tables,
        ADML_FOOTER,
    ]

    return ''.join(target_contents)


def WriteGroupPolicyTemplateAdmx(target_path, apps):
    """Writes a Group Policy template (ADM format)for the specified apps.

  The file is UTF-16 and contains CRLF on all platforms.

  Args:
    target_path: Output path of the .ADM template file.
    apps: A list of tuples containing information about each app.
        Each element of the list is a tuple of:
          * app name
          * app ID
          * optional string to append to the auto-update explanation
            - Should start with a space or double new line.
  """

    contents = GenerateGroupPolicyTemplateAdmx(apps)
    f = codecs.open(target_path, 'wb', 'utf-16')
    f.write(contents)
    f.close()


def WriteGroupPolicyTemplateAdml(target_path, apps):
    """Writes a Group Policy template (ADM format)for the specified apps.

  The file is UTF-16 and contains CRLF on all platforms.

  Args:
    target_path: Output path of the .ADM template file.
    apps: A list of tuples containing information about each app.
        Each element of the list is a tuple of:
          * app name
          * app ID
          * optional string to append to the auto-update explanation
            - Should start with a space or double new line.
  """

    contents = GenerateGroupPolicyTemplateAdml(apps)
    f = codecs.open(target_path, 'wb', 'utf-16')
    f.write(contents)
    f.close()
