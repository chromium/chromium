# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for validating concatenated URL hash prefixes."""

import hashlib
import os

# Our real time url allowlist implementation uses 16 byte hash prefixes
HASH_PREFIX_SIZE = 16

def _HasNoHashDuplicates(hashes):
    """ Returns true if there are no duplicate hash prefixes """
    url_hash_prefix_set = set()
    for i in range(0, len(hashes), HASH_PREFIX_SIZE):
        url_hash_prefix = hashes[i:i+HASH_PREFIX_SIZE]
        if url_hash_prefix in url_hash_prefix_set:
            return False
        url_hash_prefix_set.add(url_hash_prefix)
    return True

def _GetNumberOfEntries(hashes):
    """ Returns the number of hash prefixes fetched from the
    SB API
    """
    return len(hashes)/HASH_PREFIX_SIZE

def _HasValidNumberOfEntries(num_entries):
    """ Returns true if the number of hash prefixes is an
    acceptable number - between 1500 and 4000
    """
    return num_entries >= 1500 and num_entries <= 4000

def _ContainsKnownAllowlistedUrl(hashes):
    """ Returns true if the SB API response contains the
    prefix of a known allowlisted URL, 'youtube.com/'
    """
    m = hashlib.sha256()
    m.update(b'youtube.com/')
    youtube_prefix = m.digest()[0:HASH_PREFIX_SIZE]
    return hashes.find(youtube_prefix) > -1

def _ExcludesNonAllowlistedUrl(hashes):
    """ Returns true if the SB API response excludes the
    prefix of a known non-allowlisted URL, 'evil.com/'.
    """
    m = hashlib.sha256()
    m.update(b'evil.com/')
    evil_prefix = m.digest()[0:HASH_PREFIX_SIZE]
    return hashes.find(evil_prefix) == -1

def _GetCurrentHashPrefixes():
    """ Returns the hash prefixes that are currently
    stored in the asciipb file.
    """
    outfile = os.path.join(
        os.getcwd(),
        'real_time_url_allowlist.asciipb'
    )
    # Read the ASCII
    with open(outfile, 'r') as ifile:
        ascii_pb_str = ifile.read()
    # Return url_hashes from asciipb file
    url_hashes = ascii_pb_str.split('url_hashes: ')[1].encode()
    return (
        url_hashes.decode('unicode_escape')
                  .encode("raw_unicode_escape")[1:-1]
    )

def CheckHashPrefixesAreValid(new_hash_prefixes):
    """ Determines whether the provided URL hash prefixes
    are in a valid format and have values we would expect.
    """
    assert len(new_hash_prefixes) % HASH_PREFIX_SIZE == 0, (
        "Bad url_hashes - url_hashes must use " + str(HASH_PREFIX_SIZE) +
        " byte prefixes")
    assert _HasNoHashDuplicates(new_hash_prefixes), (
        "Bad url_hashes - contains duplicate hash prefixes")
    num_hashes = _GetNumberOfEntries(new_hash_prefixes)
    assert _HasValidNumberOfEntries(num_hashes), (
        "Bad url_hashes - must have between 1500 and 4000 hash prefixes"
        " and yours contained " + str(int(num_hashes)))
    assert _ContainsKnownAllowlistedUrl(new_hash_prefixes), (
        "Bad url_hashes - does not contain the hash prefix of youtube.com/"
        " which is a known allowlisted URL"
    )
    assert _ExcludesNonAllowlistedUrl(new_hash_prefixes), (
        "Bad url_hashes - contains the hash prefix of evil.com/ which is"
        " not an allowlisted URL"
    )

def CheckHashPrefixesHaveChanged(new_hash_prefixes):
    """ Checks that the new hash prefixes are different
    from the existing hash prefixes
    """
    current_pb_hashes = _GetCurrentHashPrefixes()
    if len(new_hash_prefixes) != len(current_pb_hashes):
        return True

    # Create set of current URL hash prefixes
    current_url_hash_prefix_set = set()
    for i in range(0, len(current_pb_hashes), HASH_PREFIX_SIZE):
        url_hash_prefix = current_pb_hashes[i:i+HASH_PREFIX_SIZE]
        current_url_hash_prefix_set.add(url_hash_prefix)
    # Try to find a URL hash that is not in the current version
    for j in range(0, len(new_hash_prefixes), HASH_PREFIX_SIZE):
        url_hash_prefix = new_hash_prefixes[j:j+HASH_PREFIX_SIZE]
        if url_hash_prefix not in current_url_hash_prefix_set:
            return True
    return False
