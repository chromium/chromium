#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
 Fetch the real time URL allowlist hash prefixes using the SBv4 API. Once
 we've validated the response, save these concatenated prefixes as the
 url_hashes in the real_time_url_allowlist.asciipb file.
"""

import base64
import json
import hashlib
import optparse
import os
import re
import subprocess
import sys
import traceback
import urllib.parse
import urllib.request

from validation_utils import (
    HASH_PREFIX_SIZE, CheckHashPrefixesAreValid, CheckHashPrefixesHaveChanged
)

sys.path.append(
    os.path.abspath(os.path.join(os.path.abspath(__file__),
                                 *[os.path.pardir] * 6 + ['google_apis'])))
import google_api_keys

def FetchSafeBrowsingAllowlistHashes():
    """ Return a byte sequence of concatenated 16 byte URL hash prefixes,
    fetched from the Safe Browsing API.
    """
    url = "https://safebrowsing.googleapis.com/v4/threatListUpdates:fetch?key="
    body = {
        'list_update_requests': [
            {
                'threat_type': 'HIGH_CONFIDENCE_ALLOWLIST',
                'platform_type': 'ANDROID',
                'threat_entry_type': 'URL'
            }
        ]
    }
    headers = {'content-type': 'application/json'}
    req = urllib.request.Request(
        url + google_api_keys.GetAPIKey(),
        method='POST',
        data=bytes(json.dumps(body), encoding="utf-8"),
        headers=headers
    )

    # Parse the response - should contain concatenated allowlist hash prefixes
    res = json.loads(urllib.request.urlopen(req).read())
    update_responses = res.get('listUpdateResponses')
    assert len(update_responses) > 0, (
        "Bad response from SB API - no listUpdateResponses key")
    additions = update_responses[0].get('additions')
    assert len(additions) > 0,(
        "Bad response from SB API - no additions found in listUpdateResponses")
    allowlist_hashes = additions[0].get('rawHashes').get('rawHashes')
    prefix_size = additions[0].get('rawHashes').get('prefixSize')
    assert prefix_size >= HASH_PREFIX_SIZE, (
        "Bad response from SB API - the returned prefixes are too small. "
        "They should be at least " + str(HASH_PREFIX_SIZE) + " bytes and they "
        "are " + str(prefix_size) + " bytes.")
    decoded_allowlist_hashes = base64.b64decode(allowlist_hashes)

    # Create string of concatenated 16 byte URL hash prefixes for pb file
    new_hash_prefixes = bytearray(b'')
    for i in range(0, len(decoded_allowlist_hashes), prefix_size):
        new_hash_prefixes += decoded_allowlist_hashes[i:i+HASH_PREFIX_SIZE]
    return bytes(new_hash_prefixes)

def WriteHashesToFile(hash_prefixes):
    """ Write provided hashes to the real_time_url_allowlist.asciipb file """
    outfile = os.path.join(
        os.getcwd(),
        'real_time_url_allowlist.asciipb'
    )
    # Read the ASCII
    with open(outfile, 'r') as ifile:
        ascii_pb_str = ifile.read()
    # New contents should keep version_id and scheme_id then replace url_hashes
    new_contents = (
        ascii_pb_str.split('url_hashes')[0] +
        'url_hashes: ' +
        str(hash_prefixes)[1:]
    )
    # Write new ASCII contents
    with open(outfile, 'w') as ofile:
        ofile.write(new_contents)


class StoreRealTimeUrlAllowlistPrefixes:
  def Run(self):
    parser = optparse.OptionParser()
    parser.add_option('-p', '--buildpath',
                      help='File path of the out build directory.')

    (opts, args) = parser.parse_args()
    if opts.buildpath is None:
      parser.print_help()
      return 1

    try:
      hash_prefixes = FetchSafeBrowsingAllowlistHashes()
      CheckHashPrefixesAreValid(hash_prefixes)
      assert CheckHashPrefixesHaveChanged(hash_prefixes), (
        "The URL hash prefixes have not changed, so we won't update the file"
      )
      WriteHashesToFile(hash_prefixes)
    except Exception as e:
      print("ERROR: Failed to receive valid response from SB API:\n  %s\n%s" %
            (str(e), traceback.format_exc()))
      return 1


def main():
    return StoreRealTimeUrlAllowlistPrefixes().Run()


if __name__ == '__main__':
    sys.exit(main())
