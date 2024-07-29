#!/bin/bash
#
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
if [ "$VERBOSE" ]; then
  set -x
fi
set -u

gen_spec() {
  rm -f "${SPEC}"
  # Different channels need to install to different locations so they
  # don't conflict with each other.
  local PACKAGE_ORIG="${PACKAGE}"
  local PACKAGE_FILENAME="${PACKAGE}-${CHANNEL}"
  if [ "$CHANNEL" != "stable" ]; then
    local INSTALLDIR="${INSTALLDIR}-${CHANNEL}"
    local PACKAGE="${PACKAGE}-${CHANNEL}"
    local MENUNAME="${MENUNAME} (${CHANNEL})"
  fi
  process_template "${SCRIPTDIR}/chrome.spec.template" "${SPEC}"
}

# Setup the installation directory hierarchy in the package staging area.
prep_staging_rpm() {
  prep_staging_common
  install -m 755 -d "${STAGEDIR}/etc/cron.daily"
}

# Put the package contents in the staging area.
stage_install_rpm() {
  # TODO(phajdan.jr): Deduplicate this and debian/build.sh .
  # For now duplication is going to help us avoid merge conflicts
  # as changes are frequently merged to older branches related to SxS effort.
  local PACKAGE_ORIG="${PACKAGE}"
  if [ "$CHANNEL" != "stable" ]; then
    # Avoid file collisions between channels.
    local PACKAGE="${PACKAGE}-${CHANNEL}"
    local INSTALLDIR="${INSTALLDIR}-${CHANNEL}"

    # Make it possible to distinguish between menu entries
    # for different channels.
    local MENUNAME="${MENUNAME} (${CHANNEL})"
  fi
  prep_staging_rpm
  SHLIB_PERMS=755
  stage_install_common
  log_cmd echo "Staging RPM install files in '${STAGEDIR}'..."
  process_template "${OUTPUTDIR}/installer/common/rpmrepo.cron" \
    "${STAGEDIR}/etc/cron.daily/${PACKAGE}"
  chmod 755 "${STAGEDIR}/etc/cron.daily/${PACKAGE}"
}

verify_package() {
  local DEPENDS="$1"
  local EXPECTED_DEPENDS="${TMPFILEDIR}/expected_rpm_depends"
  local ACTUAL_DEPENDS="${TMPFILEDIR}/actual_rpm_depends"
  local ADDITIONAL_RPM_DEPENDS="/bin/sh, \
  rpmlib(CompressedFileNames) <= 3.0.4-1, \
  rpmlib(FileDigests) <= 4.6.0-1, \
  rpmlib(PayloadFilesHavePrefix) <= 4.0-1, \
  /usr/sbin/update-alternatives"
  if [ ${IS_OFFICIAL_BUILD} -ne 0 ]; then
    ADDITIONAL_RPM_DEPENDS="${ADDITIONAL_RPM_DEPENDS}, \
      rpmlib(PayloadIsXz) <= 5.2-1"
  fi
  echo "${DEPENDS}" "${ADDITIONAL_RPM_DEPENDS}" | sed 's/,/\n/g' | \
      sed 's/^ *//' | LANG=C sort | uniq > "${EXPECTED_DEPENDS}"
  rpm -qpR "${OUTPUTDIR}/${PKGNAME}.${ARCHITECTURE}.rpm" | LANG=C sort | uniq \
      > "${ACTUAL_DEPENDS}"
  BAD_DIFF=0
  diff -u "${EXPECTED_DEPENDS}" "${ACTUAL_DEPENDS}" || BAD_DIFF=1
  if [ $BAD_DIFF -ne 0 ] && [ -z "${IGNORE_DEPS_CHANGES:-}" ]; then
    echo
    echo "ERROR: bad rpm dependencies!"
    echo
    exit $BAD_DIFF
  fi
}

# Actually generate the package file.
do_package() {
  log_cmd echo "Packaging ${ARCHITECTURE}..."
  PROVIDES="${PACKAGE}"
  RPM_COMMON_DEPS="${OUTPUTDIR}/rpm_common.deps"
  DEPENDS=$(cat "${RPM_COMMON_DEPS}" | tr '\n' ',')
  gen_spec

  # Create temporary rpmbuild dirs.
  mkdir -p "$RPMBUILD_DIR/BUILD"
  mkdir -p "$RPMBUILD_DIR/RPMS"

  if [ ${IS_OFFICIAL_BUILD} -ne 0 ]; then
    local COMPRESSION_OPT="_binary_payload w9.xzdio"
  else
    local COMPRESSION_OPT="_binary_payload w0.gzdio"
  fi

  # '__os_install_post ${nil}' disables a bunch of automatic post-processing
  # (brp-compress, etc.), which by default appears to only be enabled on 32-bit,
  # and which doesn't gain us anything since we already explicitly do all the
  # compression, symbol stripping, etc. that we want.
  log_cmd fakeroot rpmbuild -bb --target="$ARCHITECTURE" --rmspec \
    --define "_topdir $RPMBUILD_DIR" \
    --define "${COMPRESSION_OPT}" \
    --define "__os_install_post  %{nil}" \
    --define "_build_id_links none" \
    "${SPEC}"
  PKGNAME="${PACKAGE}-${CHANNEL}-${VERSION}-${PACKAGE_RELEASE}"
  mv "$RPMBUILD_DIR/RPMS/$ARCHITECTURE/${PKGNAME}.${ARCHITECTURE}.rpm" \
     "${OUTPUTDIR}"
  # Make sure the package is world-readable, otherwise it causes problems when
  # copied to share drive.
  chmod a+r "${OUTPUTDIR}/${PKGNAME}.${ARCHITECTURE}.rpm"

  verify_package "$DEPENDS"
}

# Remove temporary files and unwanted packaging output.
cleanup() {
  rm -rf "${STAGEDIR}"
  rm -rf "${TMPFILEDIR}"
  rm -rf "${RPMBUILD_DIR}"
}

usage() {
  echo "usage: $(basename $0) [-a target_arch] -c channel -d branding"
  echo "                      [-f] [-o 'dir'] -t target_os"
  echo "-a arch     rpm package architecture"
  echo "-c channel  the package channel (canary, unstable, beta, stable)"
  echo "-d brand    either chromium or google_chrome"
  echo "-f          indicates that this is an official build"
  echo "-h          this help message"
  echo "-o dir      package output directory [${OUTPUTDIR}]"
  echo "-t platform target platform"
}

# Check that the channel name is one of the allowable ones.
verify_channel() {
  case $CHANNEL in
    stable )
      CHANNEL=stable
      ;;
    unstable|dev|alpha )
      CHANNEL=unstable
      ;;
    testing|beta )
      CHANNEL=beta
      ;;
    canary )
      CHANNEL=canary
      ;;
    * )
      echo
      echo "ERROR: '$CHANNEL' is not a valid channel type."
      echo
      exit 1
      ;;
  esac
}

process_opts() {
  while getopts ":a:b:c:d:fho:t:" OPTNAME
  do
    case $OPTNAME in
      a )
        ARCHITECTURE="$OPTARG"
        ;;
      c )
        CHANNEL="$OPTARG"
        verify_channel
        ;;
      d )
        BRANDING="$OPTARG"
        ;;
      f )
        IS_OFFICIAL_BUILD=1
        ;;
      h )
        usage
        exit 0
        ;;
      o )
        OUTPUTDIR=$(readlink -f "${OPTARG}")
        mkdir -p "${OUTPUTDIR}"
        ;;
      t )
        TARGET_OS="$OPTARG"
        ;;
     \: )
        echo "'-$OPTARG' needs an argument."
        usage
        exit 1
        ;;
      * )
        echo "invalid command-line option: $OPTARG"
        usage
        exit 1
        ;;
    esac
  done
}

#=========
# MAIN
#=========

SCRIPTDIR=$(readlink -f "$(dirname "$0")")
OUTPUTDIR="${PWD}"

# call cleanup() on exit
trap cleanup 0
process_opts "$@"
export ARCHITECTURE="${ARCHITECTURE}"
IS_OFFICIAL_BUILD=${IS_OFFICIAL_BUILD:=0}

STAGEDIR="${OUTPUTDIR}/rpm-staging-${CHANNEL}"
mkdir -p "${STAGEDIR}"
TMPFILEDIR="${OUTPUTDIR}/rpm-tmp-${CHANNEL}"
mkdir -p "${TMPFILEDIR}"
RPMBUILD_DIR="${OUTPUTDIR}/rpm-build-${CHANNEL}"
mkdir -p "${RPMBUILD_DIR}"
SPEC="${TMPFILEDIR}/chrome.spec"

source ${OUTPUTDIR}/installer/common/installer.include

get_version_info

if [ "$BRANDING" = "google_chrome" ]; then
  source "${OUTPUTDIR}/installer/common/google-chrome.info"
else
  source "${OUTPUTDIR}/installer/common/chromium-browser.info"
fi
eval $(sed -e "s/^\([^=]\+\)=\(.*\)$/export \1='\2'/" \
  "${OUTPUTDIR}/installer/theme/BRANDING")

REPOCONFIG="https://dl.google.com/linux/${PACKAGE#google-}/rpm/stable"
verify_channel
export USR_BIN_SYMLINK_NAME="${PACKAGE}-${CHANNEL}"

stage_install_rpm
do_package
