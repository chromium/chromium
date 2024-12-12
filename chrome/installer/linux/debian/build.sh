#!/bin/bash
#
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -o pipefail
if [ "$VERBOSE" ]; then
  set -x
fi
set -u

# Create the Debian changelog file needed by dpkg-gencontrol. This just adds a
# placeholder change, indicating it is the result of an automatic build.
# TODO(mmoss) Release packages should create something meaningful for a
# changelog, but simply grabbing the actual 'svn log' is way too verbose. Do we
# have any type of "significant/visible changes" log that we could use for this?
gen_changelog() {
  rm -f "${DEB_CHANGELOG}"
  process_template "${SCRIPTDIR}/changelog.template" "${DEB_CHANGELOG}"
  debchange -a --nomultimaint -m --changelog "${DEB_CHANGELOG}" \
    "Release Notes: ${RELEASENOTES}"
  GZLOG="${STAGEDIR}/usr/share/doc/${PACKAGE}-${CHANNEL}/changelog.gz"
  mkdir -p "$(dirname "${GZLOG}")"
  gzip -9 -c "${DEB_CHANGELOG}" > "${GZLOG}"
  chmod 644 "${GZLOG}"
}

# Create the Debian control file needed by dpkg-deb.
gen_control() {
  dpkg-gencontrol -v"${VERSIONFULL}" -c"${DEB_CONTROL}" -l"${DEB_CHANGELOG}" \
  -f"${DEB_FILES}" -p"${PACKAGE}-${CHANNEL}" -P"${STAGEDIR}" \
  -O > "${STAGEDIR}/DEBIAN/control"
  rm -f "${DEB_CONTROL}"
}

# Setup the installation directory hierarchy in the package staging area.
prep_staging_debian() {
  prep_staging_common
  install -m 755 -d "${STAGEDIR}/DEBIAN" \
    "${STAGEDIR}/etc/cron.daily" \
    "${STAGEDIR}/usr/share/menu" \
    "${STAGEDIR}/usr/share/doc/${USR_BIN_SYMLINK_NAME}"
}

# Put the package contents in the staging area.
stage_install_debian() {
  # Always use a different name for /usr/bin symlink depending on channel.
  # First, to avoid file collisions. Second, to make it possible to
  # use update-alternatives for /usr/bin/google-chrome.
  local USR_BIN_SYMLINK_NAME="${PACKAGE}-${CHANNEL}"

  local PACKAGE_ORIG="${PACKAGE}"
  if [ "$CHANNEL" != "stable" ]; then
    # Avoid file collisions between channels.
    local INSTALLDIR="${INSTALLDIR}-${CHANNEL}"

    local PACKAGE="${PACKAGE}-${CHANNEL}"

    # Make it possible to distinguish between menu entries
    # for different channels.
    local MENUNAME="${MENUNAME} (${CHANNEL})"
  fi
  prep_staging_debian
  SHLIB_PERMS=644
  stage_install_common
  log_cmd echo "Staging Debian install files in '${STAGEDIR}'..."
  install -m 755 -d "${STAGEDIR}/${INSTALLDIR}/cron"
  process_template "${OUTPUTDIR}/installer/common/repo.cron" \
      "${STAGEDIR}/${INSTALLDIR}/cron/${PACKAGE}"
  chmod 755 "${STAGEDIR}/${INSTALLDIR}/cron/${PACKAGE}"
  pushd "${STAGEDIR}/etc/cron.daily/" > /dev/null
  ln -snf "${INSTALLDIR}/cron/${PACKAGE}" "${PACKAGE}"
  popd > /dev/null
  process_template "${OUTPUTDIR}/installer/debian/debian.menu" \
    "${STAGEDIR}/usr/share/menu/${PACKAGE}.menu"
  chmod 644 "${STAGEDIR}/usr/share/menu/${PACKAGE}.menu"
  process_template "${OUTPUTDIR}/installer/debian/postinst" \
    "${STAGEDIR}/DEBIAN/postinst"
  chmod 755 "${STAGEDIR}/DEBIAN/postinst"
  process_template "${OUTPUTDIR}/installer/debian/prerm" \
    "${STAGEDIR}/DEBIAN/prerm"
  chmod 755 "${STAGEDIR}/DEBIAN/prerm"
  process_template "${OUTPUTDIR}/installer/debian/postrm" \
    "${STAGEDIR}/DEBIAN/postrm"
  chmod 755 "${STAGEDIR}/DEBIAN/postrm"
}

verify_package() {
  local DEPENDS="$1"
  local EXPECTED_DEPENDS="${TMPFILEDIR}/expected_deb_depends"
  local ACTUAL_DEPENDS="${TMPFILEDIR}/actual_deb_depends"
  echo ${DEPENDS} | sed 's/, /\n/g' | LANG=C sort > "${EXPECTED_DEPENDS}"
  dpkg -I "${PACKAGE}-${CHANNEL}_${VERSIONFULL}_${ARCHITECTURE}.deb" | \
      grep '^ Depends: ' | sed 's/^ Depends: //' | sed 's/, /\n/g' | \
      LANG=C sort > "${ACTUAL_DEPENDS}"
  BAD_DIFF=0
  diff -u "${EXPECTED_DEPENDS}" "${ACTUAL_DEPENDS}" || BAD_DIFF=1
  if [ $BAD_DIFF -ne 0 ]; then
    echo
    echo "ERROR: bad dpkg dependencies!"
    echo
    exit $BAD_DIFF
  fi
}

# Actually generate the package file.
do_package() {
  log_cmd echo "Packaging ${ARCHITECTURE}..."
  PREDEPENDS="$COMMON_PREDEPS"
  DEPENDS="${COMMON_DEPS}"
  PROVIDES="www-browser"
  gen_changelog
  process_template "${SCRIPTDIR}/control.template" "${DEB_CONTROL}"
  export DEB_HOST_ARCH="${ARCHITECTURE}"
  if [ -f "${DEB_CONTROL}" ]; then
    gen_control
  fi
  log_cmd fakeroot dpkg-deb -Znone -b "${STAGEDIR}" "${TMPFILEDIR}"
  local PACKAGEFILE="${PACKAGE}-${CHANNEL}_${VERSIONFULL}_${ARCHITECTURE}.deb"
  if [ ${IS_OFFICIAL_BUILD} -ne 0 ]; then
    (cd "${TMPFILEDIR}" && ar -x "${TMPFILEDIR}/${PACKAGEFILE}")
    xz -z9 -T0 --lzma2='dict=256MiB' "${TMPFILEDIR}/data.tar"
    xz -z0 "${TMPFILEDIR}/control.tar"
    ar -d "${TMPFILEDIR}/${PACKAGEFILE}" control.tar data.tar
    ar -r "${TMPFILEDIR}/${PACKAGEFILE}" "${TMPFILEDIR}/control.tar.xz" \
      "${TMPFILEDIR}/data.tar.xz"
  fi
  mv "${TMPFILEDIR}/${PACKAGEFILE}" .
  verify_package "$DEPENDS"
}

# Remove temporary files and unwanted packaging output.
cleanup() {
  log_cmd echo "Cleaning..."
  rm -rf "${STAGEDIR}"
  rm -rf "${TMPFILEDIR}"
}

usage() {
  echo "usage: $(basename $0) [-a target_arch] -c channel -d branding"
  echo "                      [-f] [-o 'dir'] -s 'dir' -t target_os"
  echo "-a arch      deb package architecture"
  echo "-c channel   the package channel (canary, unstable, beta, stable)"
  echo "-d brand     either chromium or google_chrome"
  echo "-f           indicates that this is an official build"
  echo "-h           this help message"
  echo "-o dir       package output directory [${OUTPUTDIR}]"
  echo "-s dir       /path/to/sysroot"
  echo "-t platform  target platform"
}

# Check that the channel name is one of the allowable ones.
verify_channel() {
  case $CHANNEL in
    stable )
      CHANNEL=stable
      RELEASENOTES="https://chromereleases.googleblog.com/search/label/Stable%20updates"
      ;;
    beta|testing )
      CHANNEL=beta
      RELEASENOTES="https://chromereleases.googleblog.com/search/label/Beta%20updates"
      ;;
    dev|unstable|alpha )
      CHANNEL=unstable
      RELEASENOTES="https://chromereleases.googleblog.com/search/label/Dev%20updates"
      ;;
    # Canary is released twice a day automatically, so no release notes
    # attached.
    canary )
      CHANNEL=canary
      RELEASENOTES="N/A"
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
  while getopts ":a:b:c:d:fho:s:t:" OPTNAME
  do
    case $OPTNAME in
      a )
        ARCHITECTURE="$OPTARG"
        ;;
      c )
        CHANNEL="$OPTARG"
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
      s )
        SYSROOT="$OPTARG"
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
IS_OFFICIAL_BUILD=${IS_OFFICIAL_BUILD:=0}

STAGEDIR="${OUTPUTDIR}/deb-staging-${CHANNEL}"
mkdir -p "${STAGEDIR}"
TMPFILEDIR="${OUTPUTDIR}/deb-tmp-${CHANNEL}"
mkdir -p "${TMPFILEDIR}"
DEB_CHANGELOG="${TMPFILEDIR}/changelog"
DEB_FILES="${TMPFILEDIR}/files"
DEB_CONTROL="${TMPFILEDIR}/control"

source ${OUTPUTDIR}/installer/common/installer.include

get_version_info
VERSIONFULL="${VERSION}-${PACKAGE_RELEASE}"

if [ "$BRANDING" = "google_chrome" ]; then
  source "${OUTPUTDIR}/installer/common/google-chrome.info"
else
  source "${OUTPUTDIR}/installer/common/chromium-browser.info"
fi
eval $(sed -e "s/^\([^=]\+\)=\(.*\)$/export \1='\2'/" \
  "${OUTPUTDIR}/installer/theme/BRANDING")

verify_channel

# Some Debian packaging tools want these set.
export DEBFULLNAME="${MAINTNAME}"
export DEBEMAIL="${MAINTMAIL}"
export ARCHITECTURE="${ARCHITECTURE}"

DEB_COMMON_DEPS="${OUTPUTDIR}/deb_common.deps"
COMMON_DEPS=$(sed ':a;N;$!ba;s/\n/, /g' "${DEB_COMMON_DEPS}")
COMMON_PREDEPS="dpkg (>= 1.14.0)"

# Make everything happen in the OUTPUTDIR.
cd "${OUTPUTDIR}"
BASEREPOCONFIG="dl.google.com/linux/chrome/deb/ stable main"
# Only use the default REPOCONFIG if it's unset (e.g. verify_channel might have
# set it to an empty string)
REPOCONFIG="${REPOCONFIG-deb [arch=${ARCHITECTURE}] https://${BASEREPOCONFIG}}"
# Allowed configs include optional HTTPS support and explicit multiarch
# platforms.
REPOCONFIGREGEX="deb (\\\\[arch=[^]]*\\\\b${ARCHITECTURE}\\\\b[^]]*\\\\]"
REPOCONFIGREGEX+="[[:space:]]*) https?://${BASEREPOCONFIG}"
stage_install_debian

do_package
