#!/bin/bash
#
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# A script to setup symbolic links needed for Chrome's PyAuto framework.

ln -f -s /opt/google/chrome/chrome $(dirname $0)/chrome
[ -L $(dirname $0)/locales ] || ln -f -s /opt/google/chrome/locales \
    $(dirname $0)/locales
[ -L $(dirname $0)/resources ] || ln -f -s /opt/google/chrome/resources \
    $(dirname $0)/resources
ln -f -s /opt/google/chrome/*.pak $(dirname $0)/
