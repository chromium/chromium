#!/bin/bash

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Test of the Mac Chrome update script.
# Runs from the same directory as `keystone_install.sh` itself.
# Please note: this will destroy the brand code file for your user install of
#     Chrome, if any.


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
KSADMIN_VERSION_LIE="1.0.9.2318"

# Temp directory to be used as the disk image (source)
TEMPDIR=$(mktemp -d -t $(basename ${0}))
PATH=$PATH:"${TEMPDIR}"
OUTDIR="${TEMPDIR}/out"
OUTFILE="${OUTDIR}/register_flags.txt"

echo "using tempdir ${TEMPDIR}"

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
  assert_zero_registration_calls
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
  assert_one_registration_call
}

function prepare_fake_ksadmin() {
  if [ -e "${OUTDIR}" ] ; then
   rm -rf "${OUTDIR}"
  fi
  mkdir "${OUTDIR}"
  cat >"${TEMPDIR}"/ksadmin <<EOF
#!/bin/bash
if [ "\${1}" = "--ksadmin-version" ] ; then
  # version check
  echo "${KSADMIN_VERSION_LIE}"
  exit 0
fi
if [ "\${1}" = "-pP" ] ; then
  # finding app to update
  echo " xc=<KSPathExistenceChecker:0x45 path=${DEST}>"
  exit 0
fi
# assume it is registration; prepare to save args
# rotate last flag file
if [ -e "${OUTFILE}" ] ; then
  n=1
  while [ -e "${OUTFILE}.old.\${n}" ] ; do
   n=\$(( n+1 ))
  done
  mv "${OUTFILE}" "${OUTFILE}.old.\${n}"
fi
# now actually save args
touch "${OUTFILE}"
while (( "\$#" )) ; do
  echo "\${1}" >> "${OUTFILE}"
  shift
done
exit 0
EOF
  chmod u+x "${TEMPDIR}"/ksadmin
}

function assert_zero_registration_calls() {
  if [ -e "${OUTFILE}" ] ; then
    echo "  FAILED. ksadmin unexpectedly invoked with registration-like flags"
    echo "    See ${OUTDIR}"
    exit 1
  fi
}

function assert_one_registration_call() {
  if [ ! -e "${OUTFILE}" ] ; then
    echo "  FAILED. ksadmin not invoked for registration"
    exit 1
  fi
  if [ -e "${OUTFILE}.old.1" ] ; then
    echo "  FAILED. multiple registrations during invocation"
    echo "    See ${OUTDIR}"
    exit 1
  fi
}

function expect_registration_flag() {
  while read -r line; do
    if [ "${line}" == "${1}" ] ; then
      if read -r val && [ "${val}" == "${2}" ] ; then
        return 0
      fi
      echo "  \"${1}\" was \"${val}\", wanted \"${2}\""
      return 1
    fi
  done < "${OUTFILE}"
  echo "  \"${1}\" absent, wanted \"${2}\""
  return 1
}

function assert_registration_flags() {
  local failure=0
  while (( "$#" )) ; do
    expect_registration_flag "${1}" "${2}"
    failure=$(( $? + failure))
    shift 2
  done
  if [[ ${failure} -gt 0 ]] ; then
    echo "  FAILED. ${failure} bad flags."
    exit 1
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
  prepare_fake_ksadmin
}

# Make a new-style destination directory, to test updating between new-style
# versions.
function make_new_dest() {
  DEST="${TEMPDIR}"/Dest.app
  export KS_TICKET_XC_PATH="${DEST}"
  rm -rf "${DEST}"
  defaults write "${DEST}/Contents/Info" CFBundleShortVersionString 0
  defaults write "${DEST}/Contents/Info" KSVersion 0
  prepare_fake_ksadmin
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

function assert_registration() {
  assert_registration_flags "--productid" "com.google.Chrome" "--version" "2" \
      "--xcpath" "${DEST}" "--version-path" "${DEST}/Contents/Info.plist" \
      "--version-key" "KSVersion"
}

function assert_brand_file_registered() {
  assert_registration_flags \
      "--brand-path" "${HOME}/Library/Google/Google Chrome Brand.plist" \
      "--brand-key" "KSBrandID"
}

fail_installer "No source anything" 2

mkdir "${TEMPDIR}"/"${APPNAME_STABLE}"
fail_installer "No source bundle" 2

make_basic_src_and_dest
chmod ugo-w "${TEMPDIR}"
fail_installer "Writable dest directory" 9

make_basic_src_and_dest
fail_installer "Was no KSUpdateURL in dest after copy" 9

make_src "${APPNAME_STABLE}"
make_old_dest
defaults write "${TEMPDIR}/${APPNAME_STABLE}/Contents/Info" \
    KSUpdateURL "http://foobar"
pass_installer "Old-style update"
assert_registration

make_basic_src_and_dest
defaults write "${TEMPDIR}/${APPNAME_STABLE}/Contents/Info" \
    KSUpdateURL "http://foobar"
pass_installer "New-style Stable"
assert_registration

make_old_brand_code
make_basic_src_and_dest
defaults write "${TEMPDIR}/${APPNAME_STABLE}/Contents/Info" \
    KSUpdateURL "http://foobar"
pass_installer "Old brand code Stable"
assert_registration
assert_brand_file_registered
remove_old_brand_code

make_src "${APPNAME_CANARY}"
make_new_dest
defaults write "${TEMPDIR}/${APPNAME_CANARY}/Contents/Info" \
    KSUpdateURL "http://foobar"
pass_installer "New-style Canary"
assert_registration

cleanup_tempdir
