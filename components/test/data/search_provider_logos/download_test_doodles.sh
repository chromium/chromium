#!/bin/bash

# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script downloads a number of test doodle API responses with different
# user agents. For each response, it also extracts the fingerprint of the
# inlined image data, and calls the API again with the fingerprint value in a
# query param, to get the response without inlined image data.

# sed command to extract the fingerprint value (abc123 from
# "fingerprint":"abc123") from a given ddljson output.
FINGERPRINT_PATTERN='s/.*"fingerprint":"\([a-z0-9]*\)".*/\1/p'

# Absolute path to this folder. Output will be written there.
OUTPATH=$( cd $(dirname $0) ; pwd )

API_URL="https://www.google.com/async/ddljson?async=ntp:1"

UA_ANDROID="Mozilla/5.0%20(Linux;%20Android%206.0.1;%20Nexus%207%20Build/M4B30Q\
)%20AppleWebKit/537.36%20(KHTML,%20like%20Gecko)%20Chrome/57.0.2987.19%20Safari\
/537.36"

UA_IOS="Mozilla/5.0%20%28iPhone%3B%20CPU%20iPhone%20OS%2011_0%20like%20Mac%20OS\
%20X%29%20AppleWebKit/603.1.30%20%28KHTML%2C%20like%20Gecko%29%20CriOS/60.0.311\
2.72%20Mobile/15A5304i%20Safari/602.1"

UA_DESKTOP="Mozilla%2F5.0%20%28X11%3B%20Linux%20x86_64%29%20AppleWebKit%2F537.3\
6%20%28KHTML%2C%20like%20Gecko%29%20Chrome%2F61.0.3163.49%20Safari%2F537.36"

for i in `seq 0 4`; do
  DOODLE_PARAM="data_push_epoch=200000000$i"

  # Android UA.
  URL="$API_URL&useragent=$UA_ANDROID&$DOODLE_PARAM"
  curl $URL > $OUTPATH/ddljson\_android$i.json

  # Android UA, with fingerprint.
  FINGERPRINT=`sed -n $FINGERPRINT_PATTERN $OUTPATH/ddljson\_android$i.json`
  URL="$API_URL,es_dfp:$FINGERPRINT&useragent=$UA_ANDROID&$DOODLE_PARAM"
  curl $URL > $OUTPATH/ddljson\_android$i\_fp.json

  # iOS UA.
  URL="$API_URL&useragent=$UA_IOS&$DOODLE_PARAM"
  curl $URL > $OUTPATH/ddljson\_ios$i.json

  # iOS UA, with fingerprint.
  FINGERPRINT=`sed -n $FINGERPRINT_PATTERN $OUTPATH/ddljson\_ios$i.json`
  URL="$API_URL,es_dfp:$FINGERPRINT&useragent=$UA_IOS&$DOODLE_PARAM"
  curl $URL > $OUTPATH/ddljson\_ios$i\_fp.json

  # Desktop UA.
  URL="$API_URL&useragent=$UA_DESKTOP&$DOODLE_PARAM"
  curl $URL > $OUTPATH/ddljson\_desktop$i.json

  # Desktop UA, with fingerprint.
  FINGERPRINT=`sed -n $FINGERPRINT_PATTERN $OUTPATH/ddljson\_desktop$i.json`
  URL="$API_URL,es_dfp:$FINGERPRINT&useragent=$UA_DESKTOP&$DOODLE_PARAM"
  curl $URL > $OUTPATH/ddljson\_desktop$i\_fp.json
done

