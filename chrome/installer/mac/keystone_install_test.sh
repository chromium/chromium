#!/bin/bash

# Copyright 2009 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Test of the Mac Chrome installer.


# Where I am
DIR=$(dirname "${0}")

# My installer to test
INSTALLER="${DIR}"/keystone_install.sh
if [ ! -f "${INSTALLER}" ]; then
  echo "Can't find scripts." >& 2
  exit 1
fi

# What I test
APPNAME_STABLE="Google Chrome.app"
APPNAME_CANARY="Google Chrome Canary.app"
FWKNAME="Google Chrome Framework.framework"

# The version number for fake ksadmin to pretend to be
KSADMIN_VERSION_LIE="1.0.7.1306"

# Temp directory to be used as the disk image (source)
TEMPDIR=$(mktemp -d -t $(basename ${0}))
PATH=$PATH:"${TEMPDIR}"

# Clean up the temp directory
function cleanup_tempdir() {
  chmod u+w "${TEMPDIR}"
  rm -rf "${TEMPDIR}"
}

# Run the installer and make sure it fails.
# If it succeeds, we fail.
# Arg0: string to print
# Arg1: expected error code
function fail_installer() {
  echo $1
  "${INSTALLER}" "${TEMPDIR}" >& /dev/null
  RETURN=$?
  if [ $RETURN -eq 0 ]; then
    echo "  Did not fail (which is a failure)" >& 2
    cleanup_tempdir
    exit 1
  elif [[ $RETURN -ne $2 ]]; then
    echo "  Failed with unexpected return code ${RETURN} rather than $2" >& 2
    cleanup_tempdir
    exit 1
  else
    echo "  Successfully failed with return code ${RETURN}"
  fi
}

# Make sure installer works!
# Arg0: string to print
function pass_installer() {
  echo $1
  "${INSTALLER}" "${TEMPDIR}" >& /dev/null
  RETURN=$?
  if [ $RETURN -ne 0 ]; then
    echo "  FAILED; returned $RETURN but should have worked" >& 2
    cleanup_tempdir
    exit 1
  else
    echo "  Succeeded"
  fi
}

# Make an old-style destination directory, to test updating from old-style
# versions to new-style versions.
function make_old_dest() {
  DEST="${TEMPDIR}"/Dest.app
  export KS_TICKET_XC_PATH="${DEST}"
  rm -rf "${DEST}"
  mkdir -p "${DEST}"/Contents
  defaults write "${DEST}/Contents/Info" KSVersion 0
  cat >"${TEMPDIR}"/ksadmin <<EOF
#!/bin/sh
if [ "\${1}" = "--ksadmin-version" ] ; then
  echo "${KSADMIN_VERSION_LIE}"
  exit 0
fi
if [ -z "\${FAKE_SYSTEM_TICKET}" ] && [ "\${1}" = "-S" ] ; then
  echo no system tix! >& 2
  exit 1
fi
echo " xc=<KSPathExistenceChecker:0x45 path=${DEST}>"
exit 0
EOF
  chmod u+x "${TEMPDIR}"/ksadmin
}

# Make a new-style destination directory, to test updating between new-style
# versions.
function make_new_dest() {
  DEST="${TEMPDIR}"/Dest.app
  export KS_TICKET_XC_PATH="${DEST}"
  rm -rf "${DEST}"
  defaults write "${DEST}/Contents/Info" CFBundleShortVersionString 0
  defaults write "${DEST}/Contents/Info" KSVersion 0
  cat >"${TEMPDIR}"/ksadmin <<EOF
#!/bin/sh
if [ "\${1}" = "--ksadmin-version" ] ; then
  echo "${KSADMIN_VERSION_LIE}"
  exit 0
fi
if [ -z "\${FAKE_SYSTEM_TICKET}" ] && [ "\${1}" = "-S" ] ; then
  echo no system tix! >& 2
  exit 1
fi
echo " xc=<KSPathExistenceChecker:0x45 path=${DEST}>"
exit 0
EOF
  chmod u+x "${TEMPDIR}"/ksadmin
}

# Make a simple source directory - the update that is to be applied
# Arg0: the name of the application directory
function make_src() {
  local appname="${1}"

  chmod ugo+w "${TEMPDIR}"
  rm -rf "${TEMPDIR}/${APPNAME_STABLE}"
  rm -rf "${TEMPDIR}/${APPNAME_CANARY}"
  RSRCDIR="${TEMPDIR}/${appname}/Contents/Versions/1/${FWKNAME}/Resources"
  mkdir -p "${RSRCDIR}"
  defaults write "${TEMPDIR}/${appname}/Contents/Info" \
      CFBundleShortVersionString "1"
  defaults write "${TEMPDIR}/${appname}/Contents/Info" \
      KSProductID "com.google.Chrome"
  defaults write "${TEMPDIR}/${appname}/Contents/Info" \
      KSVersion "2"
}

function make_basic_src_and_dest() {
  make_src "${APPNAME_STABLE}"
  make_new_dest
}

function make_old_brand_code() {
  defaults write "$HOME/Library/Google/Google Chrome Brand" "KSBrandID" \
                            -string "GCCM"
}

function remove_old_brand_code() {
  rm "$HOME/Library/Google/Google Chrome Brand.plist"
}

fail_installer "No source anything" 2

mkdir "${TEMPDIR}"/"${APPNAME_STABLE}"
fail_installer "No source bundle" 2

make_basic_src_and_dest
chmod ugo-w "${TEMPDIR}"
fail_installer "Writable dest directory" 9

make_basic_src_and_dest
fail_installer "Was no KSUpdateURL in dest after copy" 9

make_basic_src_and_dest
defaults write "${TEMPDIR}/${APPNAME_STABLE}/Contents/Info" \
    KSUpdateURL "http://foobar"
export FAKE_SYSTEM_TICKET=1
fail_installer "User and system ticket both present" 4
export -n FAKE_SYSTEM_TICKET

make_src "${APPNAME_STABLE}"
make_old_dest
defaults write "${TEMPDIR}/${APPNAME_STABLE}/Contents/Info" \
    KSUpdateURL "http://foobar"
pass_installer "Old-style update"

make_basic_src_and_dest
defaults write "${TEMPDIR}/${APPNAME_STABLE}/Contents/Info" \
    KSUpdateURL "http://foobar"
pass_installer "New-style Stable"

make_old_brand_code
make_basic_src_and_dest
defaults write "${TEMPDIR}/${APPNAME_STABLE}/Contents/Info" \
    KSUpdateURL "http://foobar"
pass_installer "Old brand code Stable"
remove_old_brand_code

make_src "${APPNAME_CANARY}"
make_new_dest
defaults write "${TEMPDIR}/${APPNAME_CANARY}/Contents/Info" \
    KSUpdateURL "http://foobar"
pass_installer "New-style Canary"

cleanup_tempdir
