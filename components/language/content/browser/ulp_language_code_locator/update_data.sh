#!/bin/bash

# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script copies ULP language code locator data from a single folder
# to the right location in the Chromium source.

DIR=$1
if [ ! -d $DIR ]; then
  echo "First argument ${DIR} is not an existing directory."
  return
fi

if [ ! -d components/test/data/language ]; then
  echo "Making components/test/data/language"
  mkdir components/test/data/language
fi
for i in `seq 0 2`;
do
  cp ${DIR}/geolanguage-data_rank$i.bin \
    components/language/content/browser/ulp_language_code_locator/
  cp ${DIR}/celltolang-data_rank$i.csv components/test/data/language/
done



