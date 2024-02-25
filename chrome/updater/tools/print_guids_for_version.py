# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script that prints the generated IIDs/CLSIDs/LIBIDs for the COM interfaces,
classes, and libraries.

The entries in this file need to be kept in sync with the corresponding values
in `chrome/updater/app/server/win/BUILD.gn`.

Usage:
    python3 print_guids_for_version.py --version "119.0.5999.0"
"""

import argparse
import uuid


def _Main():
    """Prints the COM IIDs/CLSIDs/LIBIDs."""
    cmd_parser = argparse.ArgumentParser(
        description='Script to print the COM IIDs/CLSIDs/LIBIDs.')

    cmd_parser.add_argument('--version',
                            dest='version',
                            type=str,
                            required=True,
                            help='updater version to print the GUIDs for.')
    cmd_parser.add_argument(
        '--updater_product_full_name',
        default='GoogleUpdater',
        help='can be `--updater_product_full_name ChromiumUpdater`')
    flags = cmd_parser.parse_args()

    # These GUIDs must depend on branding only.
    branding_only_placeholder_guids = {
        "69464FF0-D9EC-4037-A35F-8AE4358106CC": "UpdaterLib",
        "158428a4-6014-4978-83ba-9fad0dabe791": "UpdaterUserClass",
        "415FD747-D79E-42D7-93AC-1BA6E5FD4E93": "UpdaterSystemClass",
        "63B8FFB1-5314-48C9-9C57-93EC8BC6184B": "IUpdater",
        "02AFCB67-0899-4676-91A9-67D92B3B7918": "IUpdaterUser",
        "FCE335F3-A55C-496E-814F-85971C9FA6F1": "IUpdaterSystem",
        "46ACF70B-AC13-406D-B53B-B2C4BF091FF6": "IUpdateState",
        "C3485D9F-C684-4C43-B85B-E339EA395C29": "IUpdateStateUser",
        "EA6FDC05-CDC5-4EA4-AB41-CCBD1040A2B5": "IUpdateStateSystem",
        "2FCD14AF-B645-4351-8359-E80A0E202A0B": "ICompleteStatus",
        "9AD1A645-5A4B-4D36-BC21-F0059482E6EA": "ICompleteStatusUser",
        "E2BD9A6B-0A19-4C89-AE8B-B7E9E51D9A07": "ICompleteStatusSystem",
        "7B416CFD-4216-4FD6-BD83-7C586054676E": "IUpdaterObserver",
        "B54493A0-65B7-408C-B650-06265D2182AC": "IUpdaterObserverUser",
        "057B500A-4BA2-496A-B1CD-C5DED3CCC61B": "IUpdaterObserverSystem",
        "8BAB6F84-AD67-4819-B846-CC890880FD3B": "IUpdaterCallback",
        "34ADC89D-552B-4102-8AE5-D613A691335B": "IUpdaterCallbackUser",
        "F0D6763A-0182-4136-B1FA-508E334CFFC1": "IUpdaterCallbackSystem",
        "A22AFC54-2DEF-4578-9187-DB3B24381090": "IUpdaterAppState",
        "028FEB84-44BC-4A73-A0CD-603678155CC3": "IUpdaterAppStateUser",
        "92631531-8044-46F4-B645-CDFBCCC7FA3B": "IUpdaterAppStateSystem",
        "EFE903C0-E820-4136-9FAE-FDCD7F256302": "IUpdaterAppStatesCallback",
        "BCFCF95C-DE48-4F42-B0E9-D50DB407DB53":
        "IUpdaterAppStatesCallbackUser",
        "2CB8867E-495E-459F-B1B6-2DD7FFDBD462":
        "IUpdaterAppStatesCallbackSystem",
    }

    # These GUIDs must depend on branding and version.
    branding_version_placeholder_guids = {
        "C6CE92DB-72CA-42EF-8C98-6EE92481B3C9": "UpdaterInternalLib",
        "1F87FE2F-D6A9-4711-9D11-8187705F8457": "UpdaterInternalUserClass",
        "4556BA55-517E-4F03-8016-331A43C269C9": "UpdaterInternalSystemClass",
        "526DA036-9BD3-4697-865A-DA12D37DFFCA": "IUpdaterInternal",
        "C82AFDA3-CA76-46EE-96E9-474717BFA7BA": "IUpdaterInternalUser",
        "E690EB97-6E46-4361-AF8F-90A4F5496475": "IUpdaterInternalSystem",
        "D272C794-2ACE-4584-B993-3B90C622BE65": "IUpdaterInternalCallback",
        "618D9B82-9F51-4490-AF24-BB80489E1537": "IUpdaterInternalCallbackUser",
        "7E806C73-B2A4-4BC5-BDAD-2249D87F67FC":
        "IUpdaterInternalCallbackSystem",
    }

    name = flags.updater_product_full_name
    for key, interface_name in branding_only_placeholder_guids.items():
        print(interface_name, ":",
              str(uuid.uuid5(uuid.UUID(key), name)).upper())
    name = flags.updater_product_full_name + flags.version
    for key, interface_name in branding_version_placeholder_guids.items():
        print(interface_name, ":",
              str(uuid.uuid5(uuid.UUID(key), name)).upper())


if __name__ == '__main__':
    _Main()
