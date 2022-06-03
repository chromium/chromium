#!/bin/bash

# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script allows for local TypeScript builds of cros_elements. It should be
# removed once this can be integrated with the build system.
pushd "$(dirname "$0")" > /dev/null

REWRITE_IMPORTS_SCRIPT='../../../../third_party/material_web_components/rewrite_imports.js'
MWC_DIR='../../../../third_party/material_web_components/components-chromium/node_modules'
NODE_DIR='../../../third_party/node/'

$NODE_DIR/node_modules/typescript/bin/tsc

for dir in `ls -d */`; do
  echo "Rewriting imports for $dir"
  cd $dir
  ../$NODE_DIR/node.py $REWRITE_IMPORTS_SCRIPT --basedir=$MWC_DIR *.js
  cd - > /dev/null
done

popd > /dev/null
