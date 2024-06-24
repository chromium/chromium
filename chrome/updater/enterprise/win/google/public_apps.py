#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Contains the list of supported apps that have been publicly announced.

This file is used in the creation of updater.admx/updater.adml files.
"""

# Specifies the list of supported apps that have been publicly announced.
# The list is used to create app-specific entries in the Group Policy template.
# Each element of the list is a tuple of:
# (app name, app ID, optional string to append to the auto-update explanation,
#  optional string to replace the default rollback disclaimer, Force Install
#  Machine-Wide, Force Install Per-User).
# The auto-update explanation should start with a space or double new line
# (\n\n)
# PLEASE KEEP THE LIST ALPHABETIZED BY APP NAME.
#   Exception: IMEs are at the end.
EXTERNAL_APPS = [
    ('Gears', '{283EAF47-8817-4C2B-A801-AD1FADFB7BAA}',
     ' Check https://gears.google.com/.', '', False, False),
    ('Google Ads Editor', '{F7A0263C-9459-4A49-BDD5-AA35E1C35151}', '', '',
     False, False),
    ('Google Advertising Cookie Opt-out Plugin',
     '{ADDE8406-A0F3-4AC2-8878-ADC0BD37BD86}',
     ' Check https://www.google.com/ads/preferences/plugin/.', '', False,
     False),
    ('Google Analytics Opt-out Browser Add-on (for Internet Explorer)',
     '{4CCED17F-7852-4AFC-9E9E-C89D8795BDD2}',
     ' Check https://tools.google.com/dlpage/gaoptout/.', '', False, False),
    ('Google Credential Provider for Windows (GCPW)',
     '{32987697-A14E-4B89-84D6-630D5431E831}',
     ' Check https://tools.google.com/dlpage/gcpw/.', '', True, False),
    ('Google Chrome', '{8A69D345-D564-463C-AFF1-A69D9E530F96}',
     ' Check https://www.google.com/chrome/.',
     'To make sure that users are protected by the latest security updates, we '
     'recommend that they use the latest version of Chrome Browser. If you '
     'roll back to an earlier version, you will expose your users to known '
     'security issues. Sometimes you might need to temporarily roll back to an '
     'earlier version of Chrome Browser on Windows computers. For example, '
     'your users might have problems after a Chrome Browser version update.\n\n'
     'Before you temporarily roll back users to a previous version of Chrome '
     'Browser, we recommend that you turn on Chrome sync '
     '(https://www.google.com/support/chrome/a/answer/6309115) or Roaming User '
     'Profiles (https://www.google.com/support/chrome/a/answer/7349337) for all'
     ' users in your organization. If you don\'t, users will irrevocably lose '
     'all browsing data. Use this policy at your own risk.\n\n'
     'Note: You can only roll back to Chrome Browser version 72 or later.',
     True, True),
    ('Google Chrome Beta', '{8237E44A-0054-442C-B6B6-EA0509993955}',
     ' Check https://www.google.com/chrome/browser/beta.html.',
     'To make sure that users are protected by the latest security updates, we '
     'recommend that they use the latest version of Chrome Browser. If you '
     'roll back to an earlier version, you will expose your users to known '
     'security issues. Sometimes you might need to temporarily roll back to an '
     'earlier version of Chrome Browser on Windows computers. For example, '
     'your users might have problems after a Chrome Browser version update.\n\n'
     'Before you temporarily roll back users to a previous version of Chrome '
     'Browser, we recommend that you turn on Chrome sync '
     '(https://www.google.com/support/chrome/a/answer/6309115) or Roaming User '
     'Profiles (https://www.google.com/support/chrome/a/answer/7349337) for all'
     ' users in your organization. If you don\'t, users will irrevocably lose '
     'all browsing data. Use this policy at your own risk.\n\n'
     'Note: You can only roll back to Chrome Browser version 72 or later.',
     True, True),
    ('Google Chrome Binaries', '{4DC8B4CA-1BDA-483E-B5FA-D3C12E15B62D}',
     ' Check https://chromereleases.googleblog.com/.', '', False, False),
    ('Google Chrome Canary Build', '{4EA16AC7-FD5A-47C3-875B-DBF4A2008C20}',
     ' Check https://www.google.com/chrome/browser/canary.html.',
     'To make sure that users are protected by the latest security updates, we '
     'recommend that they use the latest version of Chrome Browser. If you '
     'roll back to an earlier version, you will expose your users to known '
     'security issues. Sometimes you might need to temporarily roll back to an '
     'earlier version of Chrome Browser on Windows computers. For example, '
     'your users might have problems after a Chrome Browser version update.\n\n'
     'Before you temporarily roll back users to a previous version of Chrome '
     'Browser, we recommend that you turn on Chrome sync '
     '(https://www.google.com/support/chrome/a/answer/6309115) or Roaming User '
     'Profiles (https://www.google.com/support/chrome/a/answer/7349337) for all'
     ' users in your organization. If you don\'t, users will irrevocably lose '
     'all browsing data. Use this policy at your own risk.\n\n'
     'Note: You can only roll back to Chrome Browser version 72 or later.',
     True, True),
    ('Google Chrome Dev', '{401C381F-E0DE-4B85-8BD8-3F3F14FBDA57}',
     ' Check https://www.google.com/chrome/browser/index.html?extra=devchannel',
     'To make sure that users are protected by the latest security updates, we '
     'recommend that they use the latest version of Chrome Browser. If you '
     'roll back to an earlier version, you will expose your users to known '
     'security issues. Sometimes you might need to temporarily roll back to an '
     'earlier version of Chrome Browser on Windows computers. For example, '
     'your users might have problems after a Chrome Browser version update.\n\n'
     'Before you temporarily roll back users to a previous version of Chrome '
     'Browser, we recommend that you turn on Chrome sync '
     '(https://www.google.com/support/chrome/a/answer/6309115) or Roaming User '
     'Profiles (https://www.google.com/support/chrome/a/answer/7349337) for all'
     ' users in your organization. If you don\'t, users will irrevocably lose '
     'all browsing data. Use this policy at your own risk.\n\n'
     'Note: You can only roll back to Chrome Browser version 72 or later.',
     True, True),
    ('Google Chrome Frame', '{8BA986DA-5100-405E-AA35-86F34A02ACBF}',
     ' Check https://www.google.com/chromeframe/.', '', False, False),
    ('Google Cloud Certificate Connector',
     '{79CA0169-DEE3-4588-AB99-0FFBD277EEE0}', '', '', False, False),
    ('Legacy Browser Support', '{00EBA25B-97AB-475A-BD68-C3B6C9E94260}', '',
     '', False, False),
    ('Google Drive File Stream', '{6BBAE539-2232-434A-A4E5-9A33560C6283}',
     ' Check https://www.google.com/drive/download/.', '', True, False),
    ('Google Drive plug-in for Microsoft Office',
     '{87CD15E4-0C94-47DB-B96A-BBE485C1E31C}',
     ' Check https://tools.google.com/dlpage/driveforoffice.', '', False,
     False),
    ('Google Drive plug-in for Microsoft Office Per Machine',
     '{D936DAAE-38D4-4F72-82DD-F3824534C273}',
     ' Check https://www.google.com/support/a/answer/6165960', '', True,
     False),
    ('Google Earth', '{74AF07D8-FB8F-4D51-8AC7-927721D56EBB}',
     ' Check https://earth.google.com/.', '', True, False),
    ('Google Earth (per-user install)',
     '{0A52903D-0FBF-439A-93E4-CB609A2F63DB}',
     ' Check https://earth.google.com/.', '', False, True),
    ('Google Earth Plugin', '{2BF2CA35-CCAF-4E58-BAB7-4163BFA03B88}',
     ' Check https://code.google.com/apis/earth/.', '', False, False),
    ('Google Earth Pro', '{65E60E95-0DE9-43FF-9F3F-4F7D2DFF04B5}',
     ' Check https://earth.google.com/enterprise/earth_pro.html.', '', True,
     False),
    (u'Google Hangouts Plugin for Microsoft Outlook\u00ae',
     '{6CA106C0-DD7B-4583-8800-2F9FC892CAE7}',
     ' Check https://tools.google.com/dlpage/hangouts_outlookplugin.', '',
     False, False),
    ('Google Email Uploader', '{84F41014-78F2-4EBF-AF9B-8D7D12FCC37B}',
     ' Check https://mail.google.com/mail/help/email_uploader.html.', '',
     False, False),
    ('Google Talk Labs Edition', '{7C9D2019-25AD-4F9B-B4C4-F0F537A9626E}',
     ' Check https://www.google.com/talk/labsedition/.', '', False, False),
    ('Google Talk Plugin (Voice and Video Chat)',
     '{D0AB2EBC-931B-4013-9FEB-C9C4C2225C8C}',
     ' Check https://mail.google.com/videochat/.', '', False, False),
    ('Google Toolbar (for Firefox)', '{2CCBABCB-6427-4A55-B091-49864623C43F}',
     ' Check https://toolbar.google.com/ in Firefox.', '', False, False),
    ('Google Toolbar (for Internet Explorer)',
     '{F69EABDD-A4BB-4555-BE7E-1EA5F59BBA24}',
     ' Check https://toolbar.google.com/ in Internet Explorer.', '', False,
     False),
    ('GWT Developer Plugin For Internet Explorer',
     '{9A5E649A-EC63-4C7D-99BF-75ADB345E7E5}',
     ' Check https://gwt.google.com/missing-plugin/MissingPlugin.html.', '',
     False, False),
    ('O3D', '{70308795-045C-42DA-8F4E-D452381A7459}',
     ' Check https://code.google.com/apis/o3d/.', '', False, False),
    ('O3D Extras', '{34B2805D-C72C-4F81-AED5-5A22D1E092F1}', '', '', False,
     False),
    # IMEs.
    # All are transliteration IMEs unless otherwise specified.
    ('Google Amharic Input', '{12C37803-FC8D-48C1-A759-7C88C36BCAA4}',
     ' Check https://www.google.com/ime/transliteration/.', '', False, False),
    ('Google Arabic Input', '{49B24240-CC72-48D7-9A01-6285118C9CA9}',
     ' Check https://www.google.com/ime/transliteration/.', '', False, False),
    ('Google Bengali Input', '{446C4D62-5D85-4E6A-845E-FB19AC8C84F8}',
     ' Check https://www.google.com/ime/transliteration/.', '', False, False),
    ('Google Farsi Input', '{E0642E36-9D8E-441E-A527-683F77A50FDF}',
     ' Check https://www.google.com/ime/transliteration/.', '', False, False),
    ('Google Greek Input', '{45186F45-0E1D-49F3-A534-A52B81F60897}',
     ' Check https://www.google.com/ime/transliteration/.', '', False, False),
    ('Google Gujarati Input', '{0693199F-9DF6-4020-B760-CA993177C362}',
     ' Check https://www.google.com/ime/transliteration/.', '', False, False),
    ('Google Hebrew Input', '{49742F12-1EC3-4CBF-B942-3BDCF9875C3E}',
     ' Check https://www.google.com/ime/transliteration/.', '', False, False),
    ('Google Hindi Input', '{06A2F917-C899-44EE-8F47-5B9128D96B0A}',
     ' Check https://www.google.com/ime/transliteration/.', '', False, False),
    (
        'Google Japanese Input',  # Not a transliteration IME.
        '{DDCCD2A9-025E-4142-BCEB-F467B88CF830}',
        ' Check https://www.google.com/intl/ja/ime/.',
        '',
        False,
        False),
    ('Google Kannada Input', '{689F4361-5837-4A9C-8BF8-078D04406EC3}',
     ' Check https://www.google.com/ime/transliteration/.', '', False, False),
    ('Google Malayalam Input', '{DA2110CA-14F7-4560-A76E-D47345024C49}',
     ' Check https://www.google.com/ime/transliteration/.', '', False, False),
    ('Google Marathi Input', '{79D2E710-121A-4892-9541-66740728CEBB}',
     ' Check https://www.google.com/ime/transliteration/.', '', False, False),
    ('Google Nepali Input', '{0657DE2E-EC18-4C72-8D58-7D864EA210DE}',
     ' Check https://www.google.com/ime/transliteration/.', '', False, False),
    ('Google Oriya Input', '{EE98FBB5-62FD-4601-9CFE-0A3EB03C34FC}',
     ' Check https://www.google.com/ime/transliteration/.', '', False, False),
    ('Google Punjabi Input', '{A8DE44D0-9B9D-4EAF-BD5B-6411CE79A39E}',
     ' Check https://www.google.com/ime/transliteration/.', '', False, False),
    ('Google Russian Input', '{BBE57E48-4B5B-4346-BD7C-FF75A3AADD55}',
     ' Check https://www.google.com/ime/transliteration/.', '', False, False),
    ('Google Sanskrit Input', '{0D85AB45-243B-4C69-9F5E-7D309CF4CE33}',
     ' Check https://www.google.com/ime/transliteration/.', '', False, False),
    ('Google Serbian Input', '{3A76050C-F9E7-44AF-B463-408CEBC0A896}',
     ' Check https://www.google.com/ime/transliteration/.', '', False, False),
    ('Google Sinhalese Input', '{16BA6E00-5606-49AB-8657-A4B9809F77E7}',
     ' Check https://www.google.com/ime/transliteration/.', '', False, False),
    ('Google Tamil Input', '{7498340C-3670-47E3-82AE-1BF2B1D3FCD6}',
     ' Check https://www.google.com/ime/transliteration/.', '', False, False),
    ('Google Telugu Input', '{9FF9FAC2-A7E1-4A34-AB91-77AD18CED53F}',
     ' Check https://www.google.com/ime/transliteration/.', '', False, False),
    ('Google Tigrinya Input', '{ADC6C65A-FF16-40D4-B7F1-19E403583515}',
     ' Check https://www.google.com/ime/transliteration/.', '', False, False),
    ('Google Urdu Input', '{311963FF-A0E6-4D8E-BFC7-1C90B261180C}',
     ' Check https://www.google.com/ime/transliteration/.', '', False, False),
    # End IMEs. Add other apps above the IMEs.
]
