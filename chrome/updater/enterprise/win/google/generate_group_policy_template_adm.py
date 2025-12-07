#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates a Group Policy adm template file for updater policies.

The resulting strings and files use CRLF as required by gpedit.msc.
"""

from __future__ import print_function

import codecs
import filecmp
import os
import string
import sys

HORIZONTAL_RULE = ';%s\n' % ('-' * 78)
MAIN_POLICY_KEY = r'Software\Policies\Google\Update'

# pylint: disable-msg=C6004
HEADER = """\
CLASS MACHINE
  CATEGORY !!Cat_Google
    CATEGORY !!Cat_GoogleUpdate
      KEYNAME \"""" + MAIN_POLICY_KEY + """\"
      EXPLAIN !!Explain_GoogleUpdate
"""

PREFERENCES = """
      CATEGORY !!Cat_Preferences
        KEYNAME \"""" + MAIN_POLICY_KEY + """\"
        EXPLAIN !!Explain_Preferences

        POLICY !!Pol_AutoUpdateCheckPeriod
          #if version >= 4
            SUPPORTED !!Sup_GoogleUpdate1_2_145_5
          #endif
          EXPLAIN !!Explain_AutoUpdateCheckPeriod
          PART !!Part_AutoUpdateCheckPeriod NUMERIC
            VALUENAME AutoUpdateCheckPeriodMinutes
            DEFAULT 295   ; 4 hours 55 minutes.
            MIN 0
            MAX 43200     ; 30 days.
            SPIN 60       ; Increment in hour chunks.
          END PART
          PART !!Part_DisableAllAutoUpdateChecks CHECKBOX
            VALUENAME DisableAutoUpdateChecksCheckboxValue  ; Required, unused.
            ACTIONLISTON
              ; Writes over Part_AutoUpdateCheckPeriod. Assumes this runs last.
              VALUENAME AutoUpdateCheckPeriodMinutes VALUE NUMERIC 0
            END ACTIONLISTON
            ACTIONLISTOFF
              ; Do nothing. Let Part_AutoUpdateCheckPeriod take effect.
            END ACTIONLISTOFF
            VALUEOFF  NUMERIC 0
            VALUEON   NUMERIC 1
          END PART
        END POLICY

        POLICY !!Pol_UpdateCheckSuppressedPeriod
          #if version >= 4
            SUPPORTED !!Sup_GoogleUpdate1_3_33_5
          #endif
          EXPLAIN !!Explain_UpdateCheckSuppressedPeriod
          PART !!Part_UpdateCheckSuppressedStartHour NUMERIC
            VALUENAME UpdatesSuppressedStartHour
            DEFAULT 0
            MIN 0
            MAX 23  ; 0-23 hour
            SPIN 1
          END PART
          PART !!Part_UpdateCheckSuppressedStartMin NUMERIC
            VALUENAME UpdatesSuppressedStartMin
            DEFAULT 0
            MIN 0
            MAX 59  ; 0-59 minute in an hour
            SPIN 1
          END PART
          PART !!Part_UpdateCheckSuppressedDurationMin NUMERIC
            VALUENAME UpdatesSuppressedDurationMin
            DEFAULT 60
            MIN 1
            MAX 960  ; Maximum duration is 960 minutes = 16 hours
            SPIN 30
          END PART
        END POLICY

        POLICY !!Pol_CloudPolicyOverridesPlatformPolicy
          #if version >= 4
            SUPPORTED !!Sup_GoogleUpdate1_3_35_441
          #endif
          EXPLAIN !!Explain_CloudPolicyOverridesPlatformPolicy
          VALUENAME CloudPolicyOverridesPlatformPolicy
          VALUEOFF  NUMERIC 0
          VALUEON   NUMERIC 1
        END POLICY

        POLICY !!Pol_DownloadPreference
          #if version >= 4
            SUPPORTED !!Sup_GoogleUpdate1_3_26_0
          #endif
          EXPLAIN !!Explain_DownloadPreference
          PART !!Part_DownloadPreference DROPDOWNLIST
            VALUENAME "DownloadPreference"
            ITEMLIST
              NAME !!DownloadPreference_Cacheable VALUE "cacheable"
            END ITEMLIST
          END PART
        END POLICY

      END CATEGORY  ; Preferences

      CATEGORY !!Cat_ProxyServer
        KEYNAME \"""" + MAIN_POLICY_KEY + """\"

        POLICY !!Pol_ProxyMode
          #if version >= 4
            SUPPORTED !!Sup_GoogleUpdate1_3_21_81
          #endif
          EXPLAIN !!Explain_ProxyMode

          PART !!Part_ProxyMode  DROPDOWNLIST
            VALUENAME "ProxyMode"
            ITEMLIST
              NAME !!ProxyDisabled_DropDown VALUE "direct"
              NAME !!ProxyAutoDetect_DropDown VALUE "auto_detect"
              NAME !!ProxyPacScript_DropDown VALUE "pac_script"
              NAME !!ProxyFixedServers_DropDown VALUE "fixed_servers"
              NAME !!ProxyUseSystem_DropDown VALUE "system"
            END ITEMLIST
          END PART
        END POLICY

        POLICY !!Pol_ProxyServer
          #if version >= 4
            SUPPORTED !!Sup_GoogleUpdate1_3_21_81
          #endif
          EXPLAIN !!Explain_ProxyServer

          PART !!Part_ProxyServer  EDITTEXT
            VALUENAME "ProxyServer"
          END PART
        END POLICY

        POLICY !!Pol_ProxyPacUrl
          #if version >= 4
            SUPPORTED !!Sup_GoogleUpdate1_3_21_81
          #endif
          EXPLAIN !!Explain_ProxyPacUrl

          PART !!Part_ProxyPacUrl  EDITTEXT
            VALUENAME "ProxyPacUrl"
          END PART
        END POLICY

      END CATEGORY
"""

APPLICATIONS_HEADER = """
      CATEGORY !!Cat_Applications
        KEYNAME \"""" + MAIN_POLICY_KEY + """\"
        EXPLAIN !!Explain_Applications
"""

INSTALL_POLICY_ITEMLIST_BEGIN = """\
            ITEMLIST
              NAME  !!Name_InstallsEnabled
              VALUE NUMERIC 1
              NAME  !!Name_InstallsEnabledMachineOnly
              VALUE NUMERIC 4
              NAME  !!Name_InstallsDisabled
              VALUE NUMERIC 0"""

INSTALL_POLICY_FORCE_INSTALL_MACHINE = r"""
                NAME  !!Name_ForceInstallsMachine
                VALUE NUMERIC 5"""

INSTALL_POLICY_FORCE_INSTALL_USER = r"""
                NAME  !!Name_ForceInstallsUser
                VALUE NUMERIC 6"""

INSTALL_POLICY_ITEMLIST_APP_SPECIFIC = """\
$ForceInstalls$"""

INSTALL_POLICY_ITEMLIST_END = r"""
            END ITEMLIST
            REQUIRED"""

INSTALL_POLICY_ITEMLIST = INSTALL_POLICY_ITEMLIST_BEGIN + \
                          INSTALL_POLICY_ITEMLIST_END

INSTALL_POLICY_ITEMLIST_APP_SPECIFIC = INSTALL_POLICY_ITEMLIST_BEGIN + \
                                       INSTALL_POLICY_ITEMLIST_APP_SPECIFIC + \
                                       INSTALL_POLICY_ITEMLIST_END

UPDATE_POLICY_ITEMLIST = """\
            ITEMLIST
              NAME  !!Name_UpdatesEnabled
              VALUE NUMERIC 1
              NAME  !!Name_ManualUpdatesOnly
              VALUE NUMERIC 2
              NAME  !!Name_AutomaticUpdatesOnly
              VALUE NUMERIC 3
              NAME  !!Name_UpdatesDisabled
              VALUE NUMERIC 0
            END ITEMLIST
            REQUIRED"""

APPLICATION_DEFAULTS = ("""
        POLICY !!Pol_DefaultAllowInstallation
          #if version >= 4
            SUPPORTED !!Sup_GoogleUpdate1_2_145_5
          #endif
          EXPLAIN !!Explain_DefaultAllowInstallation
          PART !!Part_InstallPolicy DROPDOWNLIST
            VALUENAME InstallDefault
""" + INSTALL_POLICY_ITEMLIST + """
          END PART
        END POLICY

        POLICY !!Pol_DefaultUpdatePolicy
          #if version >= 4
            SUPPORTED !!Sup_GoogleUpdate1_2_145_5
          #endif
          EXPLAIN !!Explain_DefaultUpdatePolicy
          PART !!Part_UpdatePolicy DROPDOWNLIST
            VALUENAME UpdateDefault
""" + UPDATE_POLICY_ITEMLIST + """
          END PART
        END POLICY
""")

APP_POLICIES_TEMPLATE = ("""
        CATEGORY !!Cat_$AppLegalId$
          KEYNAME \"""" + MAIN_POLICY_KEY + """\"

          POLICY !!Pol_AllowInstallation
            #if version >= 4
              SUPPORTED !!Sup_GoogleUpdate1_2_145_5
            #endif
            EXPLAIN !!Explain_Install$AppLegalId$
            PART !!Part_InstallPolicy DROPDOWNLIST
              VALUENAME Install$AppGuid$
""" + INSTALL_POLICY_ITEMLIST_APP_SPECIFIC.replace('            ',
                                                   '              ') + """
            END PART
          END POLICY

          POLICY !!Pol_UpdatePolicy
            #if version >= 4
              SUPPORTED !!Sup_GoogleUpdate1_2_145_5
            #endif
            EXPLAIN !!Explain_AutoUpdate$AppLegalId$
            PART !!Part_UpdatePolicy DROPDOWNLIST
              VALUENAME Update$AppGuid$
""" + UPDATE_POLICY_ITEMLIST.replace('            ', '              ') + """
            END PART
          END POLICY

          POLICY !!Pol_TargetChannel
            #if version >= 4
              SUPPORTED !!Sup_GoogleUpdate1_3_35_453
            #endif
            EXPLAIN !!Explain_TargetChannel$AppLegalId$

            PART !!Part_TargetChannel EDITTEXT
              VALUENAME "TargetChannel$AppGuid$"
            END PART
          END POLICY

          POLICY !!Pol_TargetVersionPrefix
            #if version >= 4
              SUPPORTED !!Sup_GoogleUpdate1_3_33_5
            #endif
            EXPLAIN !!Explain_TargetVersionPrefix$AppLegalId$

            PART !!Part_TargetVersionPrefix EDITTEXT
              VALUENAME "TargetVersionPrefix$AppGuid$"
            END PART
          END POLICY

          POLICY !!Pol_RollbackToTargetVersion
            #if version >= 4
              SUPPORTED !!Sup_GoogleUpdate1_3_34_3
            #endif
            EXPLAIN !!Explain_RollbackToTargetVersion$AppLegalId$
            VALUENAME RollbackToTargetVersion$AppGuid$
            VALUEOFF  NUMERIC 0
            VALUEON   NUMERIC 1
          END POLICY

        END CATEGORY  ; $AppName$
""")

APPLICATIONS_FOOTER = """
      END CATEGORY  ; Applications

    END CATEGORY  ; GoogleUpdate

  END CATEGORY  ; Google
"""

# Policy names that are used in multiple locations.
ALLOW_INSTALLATION_POLICY = 'Allow installation'
DEFAULT_ALLOW_INSTALLATION_POLICY = ALLOW_INSTALLATION_POLICY + ' default'
UPDATE_POLICY = 'Update policy override'
TARGET_CHANNEL_POLICY = 'Target Channel override'
TARGET_VERSION_POLICY = 'Target version prefix override'
ROLLBACK_VERSION_POLICY = 'Rollback to Target version'
DEFAULT_UPDATE_POLICY = UPDATE_POLICY + ' default'

# Update policy options that are used in multiple locations.
UPDATES_ENABLED = 'Always allow updates'
AUTOMATIC_UPDATES_ONLY = 'Automatic silent updates only'
MANUAL_UPDATES_ONLY = 'Manual updates only'
UPDATES_DISABLED = 'Updates disabled'

# Category names that are used in multiple locations.
PREFERENCES_CATEGORY = 'Preferences'
PROXYSERVER_CATEGORY = 'Proxy Server'
APPLICATIONS_CATEGORY = 'Applications'

# The captions for update policy were selected such that they appear in order of
# decreasing preference when organized alphabetically in gpedit.
STRINGS_HEADER_AND_COMMON = ('\n' + HORIZONTAL_RULE + """
[strings]
Sup_GoogleUpdate1_2_145_5=At least Google Update 1.2.145.5
Sup_GoogleUpdate1_3_21_81=At least Google Update 1.3.21.81
Sup_GoogleUpdate1_3_26_0=At least Google Update 1.3.26.0
Sup_GoogleUpdate1_3_33_5=At least Google Update 1.3.33.5
Sup_GoogleUpdate1_3_34_3=At least Google Update 1.3.34.3
Sup_GoogleUpdate1_3_35_441=At least Google Update 1.3.35.441
Sup_GoogleUpdate1_3_35_453=At least Google Update 1.3.35.453

Cat_Google=Google
Cat_GoogleUpdate=Google Update
Cat_Preferences=""" + PREFERENCES_CATEGORY + """
Cat_ProxyServer=""" + PROXYSERVER_CATEGORY + """
Cat_Applications=""" + APPLICATIONS_CATEGORY + """

Pol_AutoUpdateCheckPeriod=Auto-update check period override
Pol_DownloadPreference=Download URL class override
Pol_UpdateCheckSuppressedPeriod=Time period in each day to suppress auto \
update check
Pol_CloudPolicyOverridesPlatformPolicy=Cloud Policy takes precedence over \
Group Policy
Pol_ProxyMode=Choose how to specify proxy server settings
Pol_ProxyServer=Address or URL of proxy server
Pol_ProxyPacUrl=URL to a proxy .pac file
Pol_DefaultAllowInstallation=""" + DEFAULT_ALLOW_INSTALLATION_POLICY + """
Pol_AllowInstallation=""" + ALLOW_INSTALLATION_POLICY + """
Pol_DefaultUpdatePolicy=""" + DEFAULT_UPDATE_POLICY + """
Pol_UpdatePolicy=""" + UPDATE_POLICY + """
Pol_TargetChannel=""" + TARGET_CHANNEL_POLICY + """
Pol_TargetVersionPrefix=""" + TARGET_VERSION_POLICY + """
Pol_RollbackToTargetVersion=""" + ROLLBACK_VERSION_POLICY + """

Part_AutoUpdateCheckPeriod=Minutes between update checks
Part_DownloadPreference=Type of download URL to request
Part_UpdateCheckSuppressedStartHour=Hour in a day that start to suppress \
update check
Part_UpdateCheckSuppressedStartMin=Minute in hour that starts to suppress \
update check
Part_UpdateCheckSuppressedDurationMin=Number of minutes to suppress update \
check each day
Part_DisableAllAutoUpdateChecks=Disable all periodic network traffic (not \
recommended)
Part_ProxyMode=Choose how to specify proxy server settings
Part_ProxyServer=Address or URL of proxy server
Part_ProxyPacUrl=URL to a proxy .pac file
Part_InstallPolicy=Policy
Part_UpdatePolicy=Policy
Part_TargetChannel=Target Channel
Part_TargetVersionPrefix=Target version prefix

Name_InstallsEnabled=Always allow Installs (recommended)
Name_InstallsEnabledMachineOnly=Always allow Machine-Wide Installs, but not \
Per-User Installs
Name_InstallsDisabled=Installs disabled
Name_ForceInstallsMachine=Force Installs (Machine-Wide)
Name_ForceInstallsUser=Force Installs (Per-User)

Name_UpdatesEnabled=""" + UPDATES_ENABLED + """ (recommended)
Name_ManualUpdatesOnly=""" + MANUAL_UPDATES_ONLY + """
Name_AutomaticUpdatesOnly=""" + AUTOMATIC_UPDATES_ONLY + """
Name_UpdatesDisabled=""" + UPDATES_DISABLED + """

ProxyDisabled_DropDown=Never use a proxy
ProxyAutoDetect_DropDown=Auto detect proxy settings
ProxyPacScript_DropDown=Use a .pac proxy script
ProxyFixedServers_DropDown=Use fixed proxy servers
ProxyUseSystem_DropDown=Use system proxy settings

DownloadPreference_Cacheable=Cacheable download URLs

""")

STRINGS_APP_NAME_TEMPLATE = """\
Cat_$AppLegalId$=$AppName$
"""

# pylint: disable-msg=C6310
# pylint: disable-msg=C6013

ADM_DOMAIN_REQUIREMENT_EN = """\
This policy is available only on Windows instances that are joined to a \
Microsoft Active Directory domain."""

# "application's" should be preceded by a different word in different contexts.
# The word is specified by replacing the $PreApplicationWord$ token.
STRINGS_UPDATE_POLICY_OPTIONS = """\
    \\n\\nOptions:\\
    \\n - """ + UPDATES_ENABLED + """: Updates are always applied when found, \
either by periodic update check or by a manual update check.\\
    \\n - """ + MANUAL_UPDATES_ONLY + """: Updates are only applied when the \
user does a manual update check. (Not all apps provide an interface for \
this.)\\
    \\n - """ + AUTOMATIC_UPDATES_ONLY + """: Updates are only applied when \
they are found via the periodic update check.\\
    \\n - """ + UPDATES_DISABLED + """: Never apply updates.\\
    \\n\\nIf you select manual updates, you should periodically check for \
updates using $PreApplicationWord$ application's manual update mechanism \
if available. If you disable updates, you should periodically check for \
updates and distribute them to users."""

STRINGS_COMMON_EXPLANATIONS = ("""
Explain_GoogleUpdate=Policies to control the installation and updating of \
Google applications that use Google Update/Google Installer.

""" +
                               HORIZONTAL_RULE +
                               '; ' + PREFERENCES_CATEGORY + '\n' +
                               HORIZONTAL_RULE + """
Explain_Preferences=General policies for Google Update.

Explain_AutoUpdateCheckPeriod=Minimum number of minutes between automatic \
update checks.\\n\\nSet this policy to the value 0 to disable all periodic \
network traffic by Google Update. This is not recommended, as it prevents \
Google Update itself from receiving stability and security updates.\\n\\nThe \
"Update policy override default" and per-application "Update policy override" \
settings should be used to manage application updates rather than this setting.\
\\n\\n%(domain_requirement)s

Explain_UpdateCheckSuppressedPeriod=If this setting is enabled, update checks \
will be suppressed during each day starting from Hour:Minute for a period of \
Duration (in minutes). Duration does not account for daylight savings time. So \
for instance, if the start time is 22:00, and with a duration of 480 minutes, \
the updates will be suppressed for 8 hours regardless of whether daylight \
savings time changes happen in between.\\n\\n%(domain_requirement)s

Explain_CloudPolicyOverridesPlatformPolicy=If this policy is Enabled, Cloud \
Policy settings take precedence over Group Policy settings for Google Update. \
If this policy is Not Configured or not Enabled, Group Policy takes precedence \
over Cloud Policy. This policy is only available as a mandatory machine \
platform policy and it only affects machine scope cloud policies.\\n\\n\
%(domain_requirement)s

Explain_DownloadPreference=If enabled, the Google Update server will attempt \
to provide cache-friendly URLs for update payloads in its responses.\\n\\n\
%(domain_requirement)s

Explain_ProxyMode=Allows you to specify the proxy server used by Google Update.\
\\n\\nIf you choose to never use a proxy server and always connect directly, \
all other options are ignored.\\n\\nIf you choose to use system proxy settings \
or auto detect the proxy server, all other options are ignored.\\n\\nIf you \
choose fixed server proxy mode, you can specify further options in 'Address or \
URL of proxy server'.\\n\\nIf you choose to use a .pac proxy script, you must \
specify the URL to the script in 'URL to a proxy .pac file.'\\n\\n\
%(domain_requirement)s
Explain_ProxyServer=You can specify the URL of the proxy server here.\\n\\n\
This policy only takes effect if you have selected manual proxy settings at \
'Choose how to specify proxy server settings'.\\n\\n%(domain_requirement)s
Explain_ProxyPacUrl=You can specify a URL to a proxy .pac file here.\\n\\nThis \
policy only takes effect if you have selected manual proxy settings at 'Choose \
how to specify proxy server settings'.\\n\\n%(domain_requirement)s

""" % {"domain_requirement": ADM_DOMAIN_REQUIREMENT_EN} +
                               HORIZONTAL_RULE +
                               '; ' + APPLICATIONS_CATEGORY + '\n' +
                               HORIZONTAL_RULE + """
Explain_Applications=Policies for individual applications.\\
    \\n\\nAn updated ADM template will be required to support Google \
applications released in the future.

Explain_DefaultAllowInstallation=Specifies the default behavior for whether \
Google software can be installed using Google Update/Google Installer.\\
    \\n\\nCan be overridden by the \"""" + ALLOW_INSTALLATION_POLICY + """\" \
for individual applications.\\
    \\n\\nOnly affects installation of Google software using Google \
Update/Google Installer. Cannot prevent running the application installer \
directly or installation of Google software that does not use Google \
Update/Google Installer for installation.\\
    \\n\\n%(domain_requirement)s

Explain_DefaultUpdatePolicy=Specifies the default policy for software updates \
from Google.\\
    \\n\\nCan be overridden by the \"""" \
    % {"domain_requirement": ADM_DOMAIN_REQUIREMENT_EN} +
                               UPDATE_POLICY + """\" for individual \
applications.\\
""" +
                               STRINGS_UPDATE_POLICY_OPTIONS.replace(
                               '$PreApplicationWord$', 'each') + """\\
    \\n\\nOnly affects updates for Google software that uses Google Update for \
updates. Does not prevent auto-updates of Google software that does not \
use Google Update for updates.\\
    \\n\\nUpdates for Google Update are not affected by this setting; Google \
Update will continue to update itself while it is installed.\\
    \\n\\nWARNING: Disabling updates will also prevent updates of any new \
Google applications released in the future, possibly including \
dependencies for future versions of installed applications.\\
    \\n\\n%(domain_requirement)s

""" % {"domain_requirement": ADM_DOMAIN_REQUIREMENT_EN} +
                               HORIZONTAL_RULE +
                               '; Individual Applications\n' +
                               HORIZONTAL_RULE)

DEFAULT_ROLLBACK_DISCLAIMER = """This policy is meant to serve as temporary \
measure when Enterprise Administrators need to downgrade for business reasons. \
To ensure users are protected by the latest security updates, the most recent \
version should be used. When versions are downgraded to older versions, there \
could be incompatibilities."""

FORCE_INSTALLS_MACHINE_EXPLAIN = """Force Installs (Machine-Wide): Allows \
Deploying $AppName$ to all machines where Google Update is pre-installed. \
Requires Google Update 1.3.36.82 or higher.\\n\\n"""
FORCE_INSTALLS_USER_EXPLAIN = """Force Installs (Per-User): Allows Deploying \
$AppName$ on a Per-User basis to all machines where Google Update is \
pre-installed Per-User. Requires Google Update 1.3.36.82 or higher.\\n\\n"""

STRINGS_APP_POLICY_EXPLANATIONS_TEMPLATE = (
    """
; $AppName$
Explain_Install$AppLegalId$=Specifies whether $AppName$ can be installed using \
Google Update/Google Installer.\\
    \\n\\nIf this policy is not configured, $AppName$ can be installed as \
specified by \"""" + DEFAULT_ALLOW_INSTALLATION_POLICY + """\".\\
    \\n\\n$ForceInstallsExplain$%(domain_requirement)s

Explain_AutoUpdate$AppLegalId$=Specifies how Google Update handles available \
$AppName$ updates from Google.\\
    \\n\\nIf this policy is not configured, Google Update handles available \
updates as specified by \"""" % {
        "domain_requirement": ADM_DOMAIN_REQUIREMENT_EN
    } + DEFAULT_UPDATE_POLICY + """\".\\
""" + STRINGS_UPDATE_POLICY_OPTIONS.replace('$PreApplicationWord$', 'the') +
    '$AppUpdateExplainExtra$'
) + """\\
    \\n\\n%(domain_requirement)s

Explain_TargetChannel$AppLegalId$=Specifies which Channel $AppName$ should be \
updated to.\\
    \\n\\nWhen this policy is enabled, the app will be updated to the Channel \
with this policy value.\\
    \\n\\nSome examples:\\n\\
    1) Not configured: app will be updated to the latest version available in \
the default Channel for the app.\\n\\
    2) Policy value is set to "stable": the app will be updated to the latest \
stable version.\\n\\
    2) Policy value is set to "beta": the app will be updated to the latest \
beta version.\\n\\
    3) Policy value is "dev": the app will be updated to the latest dev \
version.\\
    \\n\\n%(domain_requirement)s

Explain_TargetVersionPrefix$AppLegalId$=Specifies which version $AppName$ \
should be updated to.\\
    \\n\\nWhen this policy is enabled, the app will be updated to the version \
prefixed with this policy value.\\
    \\n\\nSome examples:\\n\\
    1) Not configured: app will be updated to the latest version available.\\n\\
    2) Policy value is set to "55.": the app will be updated to any minor \
version of 55 (e.g., 55.24.34 or 55.60.2).\\n\\
    3) Policy value is "55.2.": the app will be updated to any minor version \
of 55.2 (e.g., 55.2.34 or 55.2.2).\\n\\
    4) Policy value is "55.24.34": the app will be updated to this specific \
version only.\\
    \\n\\n%(domain_requirement)s

Explain_RollbackToTargetVersion$AppLegalId$=Specifies that Google Update \
should roll installations of $AppName$ back if the client has a higher version \
than that available.\\
    \\nIf this policy is not configured or is disabled, installs that have \
a version higher than that available will be left as-is. This could be the \
case if \"""" % {
    "domain_requirement": ADM_DOMAIN_REQUIREMENT_EN
} + TARGET_CHANNEL_POLICY + """\" is set to a Channel with a lower version, if \
\"""" + TARGET_VERSION_POLICY + """\" matches a lower version on the Channel, \
or if a user had installed a higher version.\\
    \\nIf this policy is enabled, installs that have a version higher than \
that available will be downgraded to the highest available version, \
respecting any configured target Channel and target version.\\
    \\n\\n$AppRollbackDisclaimer$\\
    \\n\\n%(domain_requirement)s
""" % {
    "domain_requirement": ADM_DOMAIN_REQUIREMENT_EN
}

# pylint: enable-msg=C6013
# pylint: enable-msg=C6310
# pylint: enable-msg=C6004


def GenerateGroupPolicyTemplate(apps):
    # pylint: disable-msg=C6114
    """Generates a Group Policy template (ADM format)for the specified apps.

  Replaces LF in strings above with CRLF as required by gpedit.msc.
  When writing the resulting contents to a file, use binary mode to ensure the
  CRLFs are preserved.

  Args:
    apps: A list of tuples containing information about each app.
        Each element of the list is a tuple of:
          * app name
          * app ID
          * optional string to append to the auto-update explanation
            - Should start with a space or double new line (\n\n).

  Returns:
    String containing the contents of the .ADM file.
  """

    # pylint: enable-msg=C6114

    def _CreateLegalIdentifier(input_string):
        """Converts input_string to a legal identifier for ADM files.

    Changes some characters that do not necessarily cause problems and may not
    handle all cases.

    Args:
      input_string: Text to convert to a legal identifier.

    Returns:
      String containing a legal identifier based on input_string.
    """

        # pylint: disable-msg=C6004
        return (input_string.replace(' ', '').replace('&', '').replace(
            '=',
            '').replace(';', '').replace(',', '').replace('.', '').replace(
                '?',
                '').replace('=', '').replace(';', '').replace("'", '').replace(
                    '"', '').replace('\\', '').replace('/', '').replace(
                        '(', '').replace(')', '').replace('[', '').replace(
                            ']', '').replace('{', '').replace('}', '').replace(
                                '-', '').replace('!', '').replace(
                                    '@', '').replace('#', '').replace(
                                        '$', '').replace('%', '').replace(
                                            '^', '').replace('*', '').replace(
                                                '+', '').replace(
                                                    u'\u00a9',
                                                    '')  # Copyright (C).
                .replace(u'\u00ae', '')  # Registered Trademark (R).
                .replace(u'\u2122', ''))  # Trademark (TM).

        # pylint: enable-msg=C6004

    def _WriteTemplateForApp(template, app):
        """Writes the text for the specified app based on the template.

    Replaces $AppName$, $AppLegalId$, $AppGuid$, and $AppUpdateExplainExtra$.

    Args:
      template: text to process and write.
      app: tuple containing information about the app.

    Returns:
      String containing a copy of the template populated with app-specific
      strings.
    """

        (app_name, app_guid, update_explain_extra, rollback_disclaimer,
         force_install_machine, force_install_user) = app

        if not rollback_disclaimer:
            rollback_disclaimer = DEFAULT_ROLLBACK_DISCLAIMER
        rollback_disclaimer = rollback_disclaimer.replace('\n', '\\n')

        force_installs = ''
        force_installs_explain = ''
        if force_install_machine:
            force_installs += INSTALL_POLICY_FORCE_INSTALL_MACHINE
            force_installs_explain += FORCE_INSTALLS_MACHINE_EXPLAIN
        if force_install_user:
            force_installs += INSTALL_POLICY_FORCE_INSTALL_USER
            force_installs_explain += FORCE_INSTALLS_USER_EXPLAIN

        # pylint: disable-msg=C6004
        return (template.replace('$ForceInstalls$', force_installs).replace(
            '$ForceInstallsExplain$',
            force_installs_explain).replace('$AppName$', app_name).replace(
                '$AppLegalId$', _CreateLegalIdentifier(app_name)).replace(
                    '$AppGuid$',
                    app_guid).replace('$AppUpdateExplainExtra$',
                                      update_explain_extra).replace(
                                          '$AppRollbackDisclaimer$',
                                          rollback_disclaimer))
        # pylint: enable-msg=C6004

    def _WriteTemplateForAllApps(template, apps):
        """Writes a copy of the template for each of the specified apps.

    Args:
      template: text to process and write.
      apps: list of tuples containing information about the apps.

    Returns:
      String containing concatenated copies of the template for each app in
      apps, each populated with the appropriate app-specific strings.
    """

        content = [_WriteTemplateForApp(template, app) for app in apps]
        return ''.join(content)

    target_contents = [
        HEADER,
        PREFERENCES,
        APPLICATIONS_HEADER,
        APPLICATION_DEFAULTS,
        _WriteTemplateForAllApps(APP_POLICIES_TEMPLATE, apps),
        APPLICATIONS_FOOTER,
        STRINGS_HEADER_AND_COMMON,
        _WriteTemplateForAllApps(STRINGS_APP_NAME_TEMPLATE, apps),
        STRINGS_COMMON_EXPLANATIONS,
        _WriteTemplateForAllApps(STRINGS_APP_POLICY_EXPLANATIONS_TEMPLATE,
                                 apps),
    ]

    # Join the sections of content then replace LF with CRLF.
    return ''.join(target_contents).replace('\n', '\r\n')


def WriteGroupPolicyTemplate(target_path, apps):
    """Writes a Group Policy template (ADM format)for the specified apps.

  The file is UTF-16 and contains CRLF on all platforms.

  Args:
    target_path: Output path of the .ADM template file.
    apps: A list of tuples containing information about each app.
        Each element of the list is a tuple of:
          * app name
          * app ID
          * optional string to append to the auto-update explanation
            - Should start with a space or double new line (\n\n).
  """  # pylint: disable-msg=C6114

    contents = GenerateGroupPolicyTemplate(apps)
    f = codecs.open(target_path, 'wb', 'utf-16')
    f.write(contents)
    f.close()
