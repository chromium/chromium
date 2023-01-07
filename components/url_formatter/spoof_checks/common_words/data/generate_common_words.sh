#!/bin/bash
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script generates common_words.gperf. See README.md for more info.

# Where to download the full wordlist if needed.
FULL_WORDLIST_URL=https://norvig.com/ngrams/count_1w.txt

# Where the wordlist is, or should be, stored on disk.
FULL_WORDLIST_PATH=${FULL_WORDLIST_PATH:-count_1w.txt}

# Where the list of brands found in the common word list is found.
BRAND_WORDLIST=${BRAND_WORDLIST:-brands_in_common_words.list}

# Where to store the output file.
OUTPUT_PATH=${OUTPUT_PATH:-common_words.gperf}

set -e

if [ ! -e $FULL_WORDLIST_PATH ]; then
  echo "= Fetching wordlist"
  wget -q -O $FULL_WORDLIST_PATH $FULL_WORDLIST_URL
  USING_TEMPORARY_WORDLIST=1
else
  echo "= Using provided wordlist"
fi

echo "= Generating regular expressions"
REGEX_TMPFILE=$(mktemp)
sed 's/^/^/; s/$/$/' $BRAND_WORDLIST > $REGEX_TMPFILE

echo "= Generating gperf list"
awk 'length($1) > 2 {print $1}' $FULL_WORDLIST_PATH \
  | grep -v -f $REGEX_TMPFILE \
  | head -n 10000 | sort \
  | awk 'BEGIN { print "%%" } { print $0", 0" } END { print "%%" }' \
  > $OUTPUT_PATH

echo "= Cleaning up"
rm $REGEX_TMPFILE
[ $USING_TEMPORARY_WORDLIST ] && rm $FULL_WORDLIST_PATH
