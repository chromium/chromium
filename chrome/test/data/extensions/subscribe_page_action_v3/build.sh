#!/bin/bash
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

closure_path=../../../../../third_party/google-closure-library
compiler_jar=compiler/closure-compiler-v20240317.jar

echo
echo '-------------------------------------------'
echo 'Building the RSS extension.'
echo '-------------------------------------------'

echo
echo 'Recreating out/ from scratch...'
rm -rf out/
mkdir out/

echo 'Copying manifest.json' && cp -r src/manifest.json out/
echo Copying _locales      && cp -r src/_locales out/_locales
echo Copying .html         && cp src/*.html out/
echo Copying .css          && cp src/*.css out/
echo Copying .png          && cp src/*.png out/

echo 'Copying javascipt files (all except iframe.js, which will be compiled).'
echo 'Copying background.js'   && cp src/background.js out/background.js
echo 'Copying common.js'       && cp src/common.js out/common.js
echo 'Copying doc_start.js'    && cp src/doc_start.js out/doc_start.js
echo 'Copying feed_finder.js'  && cp src/feed_finder.js out/feed_finder.js
echo 'Copying options.js'      && cp src/options.js out/options.js
echo 'Copying popup.js'        && cp src/popup.js out/popup.js
echo 'Copying sniff_common.js' && cp src/sniff_common.js out/sniff_common.js
echo 'Copying subscribe.js'    && cp src/subscribe.js out/subscribe.js
# NOTE: test_*.js files should not be copied over.
echo

# Uncomment to see hashes for scripts.
# echo 'SHA hashes:'
# echo 'subscribe.js       sha256-'$(cat src/subscribe.js | openssl dgst -sha256 -binary | openssl enc -base64)
# echo 'freeflow foo       sha256-'$(echo -n "console.log('foo');" | openssl dgst -sha256 -binary | openssl enc -base64)
# echo

echo 'Closure compiling:'
echo 'Using' $compiler_jar
echo '      ^ Make sure this is present and up to date.'

echo 'Compiling iframe.js.'
$closure_path/closure/bin/build/closurebuilder.py \
  --root=src/ \
  --root=$closure_path \
  --namespace="RSSExtension.IFrame" \
  --output_mode=compiled \
  --compiler_jar=$compiler_jar \
  > out/iframe.js


echo
echo 'The contents of out/ now contain the extension to be uploaded.'
echo

echo 'Extension:'
grep "\"version\"" out/manifest.json
echo '              ^ Please make sure this version is correct!!'

