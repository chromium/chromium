#!/bin/bash
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to release ChromeDriver, by copying it from chrome-unsigned bucket to
# chromedriver bucket.

if [[ $(uname -s) != Linux* ]]
then
  echo Please run release.sh on Linux
  exit 1
fi

if [[ $# -ne 1 || -z $1 ]]
then
  echo usage: $0 version
  exit 1
fi

origdir=`pwd`
workdir=`mktemp -d`
if [[ -z $workdir ]]
then
  echo Unable to create working directory, exiting
  exit 1
fi
echo Working directory: $workdir

cd $workdir
if [[ `pwd` != $workdir ]]
then
  echo Failed to chdir to working directory, exiting
  exit 1
fi

version=$1
src=from
tgt=to

rm -rf $src
rm -rf $tgt
rm -rf chromedriver_linux64
rm -rf chromedriver_mac64
rm -rf chromedriver_mac64_m1
rm -rf chromedriver_win32

mkdir $src
mkdir $tgt
mkdir chromedriver_mac64_m1

gsutil cp gs://chrome-unsigned/desktop-5c0tCh/$version/linux64/chromedriver_linux64.zip $src
gsutil cp gs://chrome-unsigned/desktop-5c0tCh/$version/mac64/chromedriver_mac64.zip $src
gsutil cp gs://chrome-unsigned/desktop-5c0tCh/$version/mac-arm64/chromedriver_mac64.zip $src/chromedriver_mac64_m1.zip
gsutil cp gs://chrome-unsigned/desktop-5c0tCh/$version/win-clang/chromedriver_win32.zip $src

unzip $src/chromedriver_linux64.zip
unzip $src/chromedriver_mac64.zip
unzip $src/chromedriver_mac64_m1.zip -d chromedriver_mac64_m1/
unzip $src/chromedriver_win32.zip

strip -p chromedriver_linux64/chromedriver

zip -j $tgt/chromedriver_linux64.zip chromedriver_linux64/chromedriver
zip -j $tgt/chromedriver_mac64.zip chromedriver_mac64/chromedriver
zip -j $tgt/chromedriver_mac64_m1.zip chromedriver_mac64_m1/chromedriver_mac64/chromedriver
zip -j $tgt/chromedriver_win32.zip chromedriver_win32/chromedriver.exe

gsutil cp $tgt/chromedriver_linux64.zip gs://chromedriver/$version/chromedriver_linux64.zip
gsutil cp $tgt/chromedriver_mac64.zip gs://chromedriver/$version/chromedriver_mac64.zip
gsutil cp $tgt/chromedriver_mac64_m1.zip gs://chromedriver/$version/chromedriver_mac64_m1.zip
gsutil cp $tgt/chromedriver_win32.zip gs://chromedriver/$version/chromedriver_win32.zip

echo -n $version > latest

build=`echo $version | sed -E 's/\.[0-9]+$//'`
major=`echo $version | sed -E 's/\.[0-9.]+$//'`
gsutil -h Content-Type:text/plain cp latest gs://chromedriver/LATEST_RELEASE_$build
gsutil -h Content-Type:text/plain cp latest gs://chromedriver/LATEST_RELEASE_$major

if [[ -f $origdir/notes.txt ]]
then
  gsutil cp $origdir/notes.txt gs://chromedriver/$version/notes.txt
fi
