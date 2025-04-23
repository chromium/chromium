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
KSADMIN_VERSION_LIE="137.0.7106.0"

# Temp directory to be used as the disk image (source)
TEMPDIR=$(mktemp -d -t $(basename ${0}))
OUTDIR="${TEMPDIR}/out"
OUTFILE="${OUTDIR}/register_flags.txt"

# The PATH created here must be kept in sync with PATH override behavior
# in chrome/updater/mac/install_from_archive.mm::RunInstaller, so the
# test will behave realistically.
# LINT.IfChange(InstallerEnvPath)
INSTALLER_ENV_PATH="/bin:/usr/bin:${TEMPDIR}"
# LINT.ThenChange(/chrome/updater/mac/install_from_archive.mm:InstallerEnvPath)

# Make sure there isn't some other ksadmin on the path, which would prevent
# us from reaching the fake ksadmin the tests rely on. We haven't created
# the fake yet, so we should not find anything when we ask.
bad_ksadmin="$(PATH=${INSTALLER_ENV_PATH} command -v ksadmin)"
if [ -n "${bad_ksadmin}" ] ; then
  echo "CANNOT RUN TESTS: a ksadmin at ${bad_ksadmin} conflicts with our fakes"
  exit 2
fi

LIBRARY_BRAND_DEFAULTS_TARGET="${HOME}/Library/Google/Google Chrome Brand"
LIBRARY_BRAND_FILE="${LIBRARY_BRAND_DEFAULTS_TARGET}.plist"

echo "using tempdir ${TEMPDIR}"

# Clean up the temp directory
function cleanup_tempdir() {
  chmod u+w "${TEMPDIR}"
  if [ -n "${DEST}" ] ; then
    chmod u+w "${DEST}"
  fi
  rm -rf "${TEMPDIR}"
}

function invoke_installer() {
  GOOGLE_CHROME_UPDATER_DEBUG=y PATH="${INSTALLER_ENV_PATH}" \
      "${INSTALLER}" "${TEMPDIR}" \
      > "${OUTDIR}/keystone_install.out" \
      2> "${OUTDIR}/keystone_install.err"
  RETURN=$?
}

# Run the installer and make sure it fails.
# If it succeeds, we fail.
# Arg0: string to print
# Arg1: expected error code
function fail_installer() {
  echo $1
  invoke_installer
  if [ $RETURN -eq 0 ]; then
    echo "  Did not fail (which is a failure)" >& 2
    exit 1
  elif [[ $RETURN -ne $2 ]]; then
    echo "  Failed with unexpected return code ${RETURN} rather than $2" >& 2
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
  invoke_installer
  if [ $RETURN -ne 0 ]; then
    echo "  FAILED; returned $RETURN but should have worked" >& 2
    exit 1
  else
    echo "  Succeeded"
  fi
  assert_one_registration_call
}

function reset_outdir() {
  if [ -e "${OUTDIR}" ] ; then
   rm -rf "${OUTDIR}"
  fi
  mkdir "${OUTDIR}"
}

function prepare_fake_ksadmin() {
  reset_outdir
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
if [ "\${1}" = "--print-xattr-tag-brand" ] ; then
  # xattr brand
  if [ -z "${XATTR_BRAND}" ] ; then
    echo "No xattr brand in this test" >& 2
    exit 1
  fi
  echo "${XATTR_BRAND}"
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

function set_info_plist_item() {
  defaults write "${1}/Contents/Info" "${2}" -string "${3}"
}

function set_src_ksupdateurl () {
  set_info_plist_item "${SRC}" KSUpdateURL \
      "https://example.com/update2"
}

# Make an old-style destination directory, to test updating from old-style
# versions to new-style versions.
function make_old_dest() {
  DEST="${TEMPDIR}"/Dest.app
  export KS_TICKET_XC_PATH="${DEST}"
  rm -rf "${DEST}"
  mkdir -p "${DEST}"/Contents
  set_info_plist_item "${DEST}" KSVersion 0
  prepare_fake_ksadmin
}

# Make a new-style destination directory, to test updating between new-style
# versions.
function make_new_dest() {
  DEST="${TEMPDIR}"/Dest.app
  export KS_TICKET_XC_PATH="${DEST}"
  rm -rf "${DEST}"
  set_info_plist_item "${DEST}" CFBundleShortVersionString 0
  set_info_plist_item "${DEST}" KSVersion 0
  prepare_fake_ksadmin
}

# Make a simple source directory - the update that is to be applied
# Arg0: the name of the application directory
function make_src() {
  SRC="${TEMPDIR}/${1}"

  chmod ugo+w "${TEMPDIR}"
  rm -rf "${TEMPDIR}/${APPNAME_STABLE}"
  rm -rf "${TEMPDIR}/${APPNAME_CANARY}"
  RSRCDIR="${SRC}/Contents/Versions/1/${FWKNAME}/Resources"
  mkdir -p "${RSRCDIR}"
  set_info_plist_item "${SRC}" CFBundleShortVersionString 1
  set_info_plist_item "${SRC}" KSProductID "com.google.Chrome"
  set_info_plist_item "${SRC}" KSVersion 2
}

function make_basic_src_and_dest() {
  make_src "${APPNAME_STABLE}"
  make_new_dest
}

function set_library_brand() {
  defaults write "${LIBRARY_BRAND_DEFAULTS_TARGET}" KSBrandID -string "${1}"
}

function remove_library_brand() {
  rm "${LIBRARY_BRAND_FILE}"
}

function set_dest_plist_brand() {
  set_info_plist_item "${DEST}" KSBrandID "${1}"
}

# Reads an item out of a plist file, forcing the defaults utility to read off
# the disk instead of using cached data from cfprefsd. Only use this for
# reading from actual plists.
function plist_read() {
  __CFPREFERENCES_AVOID_DAEMON=1 defaults read "${@}" 2> /dev/null
}

function assert_library_brand_registered() {
  assert_brand_file_registered
  if [ ! -e "${LIBRARY_BRAND_FILE}" ] ; then
    echo "  FAILED. No brand file at ${LIBRARY_BRAND_FILE}"
    exit 1
  fi
  local got_brand="$(plist_read \
      "${LIBRARY_BRAND_DEFAULTS_TARGET}" "KSBrandID")"
  if [ "${got_brand}" != "${1}" ] ; then
    echo "  FAILED. Wanted brand \"${1}\", got brand \"${got_brand}.\""
    exit 1
  fi
}

function assert_registration() {
  assert_registration_flags "--productid" "com.google.Chrome" "--version" "2" \
      "--xcpath" "${DEST}" "--version-path" "${DEST}/Contents/Info.plist" \
      "--version-key" "KSVersion"
}

function assert_brand_file_registered() {
  assert_registration_flags \
      "--brand-path" "${LIBRARY_BRAND_FILE}" \
      "--brand-key" "KSBrandID"
}

reset_outdir
fail_installer "No source anything" 2

mkdir "${TEMPDIR}"/"${APPNAME_STABLE}"
fail_installer "No source bundle" 2

make_basic_src_and_dest
chmod ugo-w "${DEST}"
fail_installer "Writable dest directory" 9
chmod ugo+w "${DEST}"

make_basic_src_and_dest
fail_installer "Was no KSUpdateURL in dest after copy" 9

make_src "${APPNAME_STABLE}"
make_old_dest
set_src_ksupdateurl
pass_installer "Old-style update"
assert_registration

make_basic_src_and_dest
set_src_ksupdateurl
pass_installer "New-style Stable"
assert_registration

set_library_brand "LIBR"
make_basic_src_and_dest
set_src_ksupdateurl
pass_installer "Old brand code Stable"
assert_registration
assert_library_brand_registered "LIBR"
remove_library_brand

make_src "${APPNAME_CANARY}"
make_new_dest
set_src_ksupdateurl
pass_installer "New-style Canary"
assert_registration

make_basic_src_and_dest
set_src_ksupdateurl
set_dest_plist_brand "PLST"
pass_installer "brand code in Info.plist"
assert_registration
assert_library_brand_registered "PLST"
remove_library_brand

# In a brand conflict between the library brand (presumably configured during
# a previous update) and the Info.plist brand, Info.plist should "win".
make_basic_src_and_dest
set_src_ksupdateurl
set_dest_plist_brand "PLST"
set_library_brand "LIBR"
pass_installer "conflict between library brand and Info.plist"
assert_registration
assert_library_brand_registered "PLST"
remove_library_brand

XATTR_BRAND="XATR"  # persists for remaining tests
make_basic_src_and_dest
set_src_ksupdateurl
pass_installer "brand code in xattr tag (only)"
assert_registration
assert_library_brand_registered "XATR"
remove_library_brand

make_basic_src_and_dest
set_src_ksupdateurl
set_dest_plist_brand "PLST"
pass_installer "brand code conflict between xattr tag and Info.plist"
assert_registration
assert_library_brand_registered "XATR"
remove_library_brand

make_basic_src_and_dest
set_src_ksupdateurl
set_library_brand "LIBR"
pass_installer "conflict between library brand and xattr tag"
assert_registration
assert_library_brand_registered "XATR"
remove_library_brand

make_basic_src_and_dest
set_src_ksupdateurl
set_dest_plist_brand "PLST"
set_library_brand "LIBR"
pass_installer "conflict between library brand and xattr tag"
assert_registration
assert_library_brand_registered "XATR"
remove_library_brand

cleanup_tempdir
