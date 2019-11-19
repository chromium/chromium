#!/bin/bash
# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
CHROME_SRC="$(dirname "$0")/../../.."

if [ $# -lt 1 ] ; then
  echo "Usage: "$0" [--debug-libs] stack-trace-file"
  echo "   --debug-libs uses Debug (rather than Release) libraries."
  exit 1
fi

LIB_TYPE=Release
if [[ "$1" == "--debug-libs" ]] ; then
  LIB_TYPE=Debug
  shift
fi

if [ ! -f "$1" ] ; then
  echo "$1: file not found"
  exit 1
fi

# try to automatically identify architecture and version, if there are no user-
# supplied values.
if [ -z $ARCH ] ; then
  if grep -q arm64 "$1" ; then
    ARCH=arm64-v8a
  elif grep -q armeabi-v7a "$1" ; then
    ARCH=armeabi-v7a
  elif grep -q armeabi "$1" ; then
    ARCH=armeabi
  elif grep -q "ABI: 'arm'" "$1" ; then
    ARCH=armeabi-v7a
  elif grep -q "ABI: 'arm64'" "$1" ; then
    ARCH=arm64-v8a
  elif grep -q "ABI: 'x86_64'" "$1" ; then
    ARCH=x86_64
  elif grep -q "ABI: 'x86'" "$1" ; then
    ARCH=x86
  else
    echo "Cannot determine architecture."
    echo "Set the ARCH environment variable explicitly to continue."
    exit 1
  fi
fi
if [ -z "$VERSION" ] ; then
  VERSION=$(grep -o -m1 'libcronet\..*\.so' "$1" |
            sed 's/libcronet\.\(.*\)\.so/\1/')
fi

echo VERSION=$VERSION
echo ARCH=$ARCH
echo Using symbolicator from: $CHROME_SRC
echo

ARCHOPT=
if [[ "$ARCH" == "arm64-v8a" ]] ; then
  ARCHOPT="--arch=arm64"
fi

FILE=${VERSION}/${LIB_TYPE}/cronet/symbols/${ARCH}/libcronet.${VERSION}.so
GSUTIL="$CHROME_SRC/third_party/depot_tools/gsutil.py"
$GSUTIL -m cp -R gs://chromium-cronet/android/${FILE} ~/Downloads

TRACER="$CHROME_SRC/third_party/android_platform/development/scripts/stack"
CHROMIUM_OUTPUT_DIR="$HOME/Downloads" "$TRACER" $ARCHOPT "$1"
