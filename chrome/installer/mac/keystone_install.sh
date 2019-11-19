#!/bin/bash -p

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# usage: keystone_install.sh update_dmg_mount_point
#
# Called by the Keystone system to update the installed application with a new
# version from a disk image.
#
# Environment variables:
# GOOGLE_CHROME_UPDATER_DEBUG
#   When set to a non-empty value, additional information about this script's
#   actions will be logged to stderr.  The same debugging information will
#   also be enabled when "Library/Google/Google Chrome Updater Debug" in the
#   root directory or in ${HOME} exists.
# GOOGLE_CHROME_UPDATER_TEST_PATH
#   When set to a non-empty value, the product at this path will be updated.
#   ksadmin will not be consulted to locate the installed product, nor will it
#   be called to update any tickets.
#
# Exit codes:
#  0  Happiness
#  1  Unknown failure
#  2  Basic sanity check source failure (e.g. no app on disk image)
#  3  Basic sanity check destination failure (e.g. ticket points to nothing)
#  4  Update driven by user ticket when a system ticket is also present
#  5  Could not prepare existing installed version to receive update
#  6  Patch sanity check failure
#  7  rsync failed (could not copy new versioned directory to Versions)
#  8  rsync failed (could not update outer .app bundle)
#  9  Could not get the version, update URL, or channel after update
# 10  Updated application does not have the version number from the update
# 11  ksadmin failure
# 12  dirpatcher failed for versioned directory
# 13  dirpatcher failed for outer .app bundle
# 14  The update is incompatible with the system (presently unused)
#
# The following exit codes can be used to convey special meaning to Keystone.
# KeystoneRegistration will present these codes to Chrome as "success."
# 66  (unused) success, request reboot
# 77  (unused) try installation again later

set -eu

# http://b/2290916: Keystone runs the installation with a restrictive PATH
# that only includes the directory containing ksadmin, /bin, and /usr/bin.  It
# does not include /sbin or /usr/sbin.  This script uses lsof, which is in
# /usr/sbin, and it's conceivable that it might want to use other tools in an
# sbin directory.  Adjust the path accordingly.
export PATH="${PATH}:/sbin:/usr/sbin"

# Environment sanitization.  Clear environment variables that might impact the
# interpreter's operation.  The |bash -p| invocation on the #! line takes the
# bite out of BASH_ENV, ENV, and SHELLOPTS (among other features), but
# clearing them here ensures that they won't impact any shell scripts used as
# utility programs. SHELLOPTS is read-only and can't be unset, only
# unexported.
unset BASH_ENV CDPATH ENV GLOBIGNORE IFS POSIXLY_CORRECT
export -n SHELLOPTS

set -o pipefail
shopt -s nullglob

ME="$(basename "${0}")"
readonly ME

readonly KS_CHANNEL_KEY="KSChannelID"

# Workaround for https://crbug.com/83180#c3: in bash 4.0, "declare VAR" no
# longer initializes VAR if not already set. (Apple has never shipped a bash
# newer than 3.2, but a small number of people seem to have replaced their
# system /bin/sh with a newer bash, probably all before SIP became a thing.)
: ${GOOGLE_CHROME_UPDATER_DEBUG:=}
: ${GOOGLE_CHROME_UPDATER_TEST_PATH:=}

err() {
  local error="${1}"

  local id=
  if [[ -n "${GOOGLE_CHROME_UPDATER_DEBUG}" ]]; then
    id=": ${$} $(date "+%Y-%m-%d %H:%M:%S %z")"
  fi

  echo "${ME}${id}: ${error}" >& 2
}

note() {
  local message="${1}"

  if [[ -n "${GOOGLE_CHROME_UPDATER_DEBUG}" ]]; then
    err "${message}"
  fi
}

g_temp_dir=
cleanup() {
  local status=${?}

  trap - EXIT
  trap '' HUP INT QUIT TERM

  if [[ ${status} -ge 128 ]]; then
    err "Caught signal $((${status} - 128))"
  fi

  if [[ -n "${g_temp_dir}" ]]; then
    rm -rf "${g_temp_dir}"
  fi

  exit ${status}
}

ensure_temp_dir() {
  if [[ -z "${g_temp_dir}" ]]; then
    # Choose a template that won't be a dot directory.  Make it safe by
    # removing leading hyphens, too.
    local template="${ME}"
    if [[ "${template}" =~ ^[-.]+(.*)$ ]]; then
      template="${BASH_REMATCH[1]}"
    fi
    if [[ -z "${template}" ]]; then
      template="keystone_install"
    fi

    g_temp_dir="$(mktemp -d -t "${template}")"
    note "g_temp_dir = ${g_temp_dir}"
  fi
}

# Returns 0 (true) if |symlink| exists, is a symbolic link, and appears
# writable on the basis of its POSIX permissions.  This is used to determine
# writability like test's -w primary, but -w resolves symbolic links and this
# function does not.
is_writable_symlink() {
  local symlink="${1}"

  local link_mode
  link_mode="$(stat -f %Sp "${symlink}" 2> /dev/null || true)"
  if [[ -z "${link_mode}" ]] || [[ "${link_mode:0:1}" != "l" ]]; then
    return 1
  fi

  local link_user link_group
  link_user="$(stat -f %u "${symlink}" 2> /dev/null || true)"
  link_group="$(stat -f %g "${symlink}" 2> /dev/null || true)"
  if [[ -z "${link_user}" ]] || [[ -z "${link_group}" ]]; then
    return 1
  fi

  # If the users match, check the owner-write bit.
  if [[ ${EUID} -eq "${link_user}" ]]; then
    if [[ "${link_mode:2:1}" = "w" ]]; then
      return 0
    fi
    return 1
  fi

  # If the file's group matches any of the groups that this process is a
  # member of, check the group-write bit.
  local group_match=
  local group
  for group in "${GROUPS[@]}"; do
    if [[ "${group}" -eq "${link_group}" ]]; then
      group_match="y"
      break
    fi
  done
  if [[ -n "${group_match}" ]]; then
    if [[ "${link_mode:5:1}" = "w" ]]; then
      return 0
    fi
    return 1
  fi

  # Check the other-write bit.
  if [[ "${link_mode:8:1}" = "w" ]]; then
    return 0
  fi

  return 1
}

# If |symlink| exists and is a symbolic link, but is not writable according to
# is_writable_symlink, this function attempts to replace it with a new
# writable symbolic link.  If |symlink| does not exist, is not a symbolic
# link, or is already writable, this function does nothing.  This function
# always returns 0 (true).
ensure_writable_symlink() {
  local symlink="${1}"

  if [[ -L "${symlink}" ]] && ! is_writable_symlink "${symlink}"; then
    # If ${symlink} refers to a directory, doing this naively might result in
    # the new link being placed in that directory, instead of replacing the
    # existing link.  ln -fhs is supposed to handle this case, but it does so
    # by unlinking (removing) the existing symbolic link before creating a new
    # one.  That leaves a small window during which the symbolic link is not
    # present on disk at all.
    #
    # To avoid that possibility, a new symbolic link is created in a temporary
    # location and then swapped into place with mv.  An extra temporary
    # directory is used to convince mv to replace the symbolic link: again, if
    # the existing link refers to a directory, "mv newlink oldlink" will
    # actually leave oldlink alone and place newlink into the directory.
    # "mv newlink dirname(oldlink)" works as expected, but in order to replace
    # oldlink, newlink must have the same basename, hence the temporary
    # directory.

    local target
    target="$(readlink "${symlink}" 2> /dev/null || true)"
    if [[ -z "${target}" ]]; then
      return 0
    fi

    # Error handling strategy: if anything fails, such as the mktemp, ln,
    # chmod, or mv, ignore the failure and return 0 (success), leaving the
    # existing state with the non-writable symbolic link intact.  Failures
    # in this function will be difficult to understand and diagnose, and a
    # non-writable symbolic link is not necessarily fatal.  If something else
    # requires a writable symbolic link, allowing it to fail when a symbolic
    # link is not writable is easier to understand than bailing out of the
    # script on failure here.

    local symlink_dir temp_link_dir temp_link
    symlink_dir="$(dirname "${symlink}")"
    temp_link_dir="$(mktemp -d "${symlink_dir}/.symlink_temp.XXXXXX" || true)"
    if [[ -z "${temp_link_dir}" ]]; then
      return 0
    fi
    temp_link="${temp_link_dir}/$(basename "${symlink}")"

    (ln -fhs "${target}" "${temp_link}" &&
        chmod -h 755 "${temp_link}" &&
        mv -f "${temp_link}" "${symlink_dir}/") || true
    rm -rf "${temp_link_dir}"
  fi

  return 0
}

# ensure_writable_symlinks_recursive calls ensure_writable_symlink for every
# symbolic link in |directory|, recursively.
#
# In some very weird and rare cases, it is possible to wind up with a user
# installation that contains symbolic links that the user does not have write
# permission over.  More on how that might happen later.
#
# If a weird and rare case like this is observed, rsync will exit with an
# error when attempting to update the times on these symbolic links.  rsync
# may not be intelligent enough to try creating a new symbolic link in these
# cases, but this script can be.
#
# The problem occurs when an administrative user first drag-installs the
# application to /Applications, resulting in the program's user being set to
# the user's own ID.  If, subsequently, a .pkg package is installed over that,
# the existing directory ownership will be preserved, but file ownership will
# be changed to whatever is specified by the package, typically root.  This
# applies to symbolic links as well.  On a subsequent update, rsync will be
# able to copy the new files into place, because the user still has permission
# to write to the directories.  If the symbolic link targets are not changing,
# though, rsync will not replace them, and they will remain owned by root.
# The user will not have permission to update the time on the symbolic links,
# resulting in an rsync error.
ensure_writable_symlinks_recursive() {
  local directory="${1}"

  # This fix-up is not necessary when running as root, because root will
  # always be able to write everything needed.
  if [[ ${EUID} -eq 0 ]]; then
    return 0
  fi

  # This step isn't critical.
  local set_e=
  if [[ "${-}" =~ e ]]; then
    set_e="y"
    set +e
  fi

  # Use find -print0 with read -d $'\0' to handle even the weirdest paths.
  local symlink
  while IFS= read -r -d $'\0' symlink; do
    ensure_writable_symlink "${symlink}"
  done < <(find "${directory}" -type l -print0)

  # Go back to how things were.
  if [[ -n "${set_e}" ]]; then
    set -e
  fi
}

# is_version_ge accepts two version numbers, left and right, and performs a
# piecewise comparison determining the result of left >= right, returning true
# (0) if left >= right, and false (1) if left < right. If left or right are
# missing components relative to the other, the missing components are assumed
# to be 0, such that 10.6 == 10.6.0.
is_version_ge() {
  local left="${1}"
  local right="${2}"

  local -a left_array right_array
  IFS=. left_array=(${left})
  IFS=. right_array=(${right})

  local left_count=${#left_array[@]}
  local right_count=${#right_array[@]}
  local count=${left_count}
  if [[ ${right_count} -lt ${count} ]]; then
    count=${right_count}
  fi

  # Compare the components piecewise, as long as there are corresponding
  # components on each side. If left_element and right_element are unequal,
  # a comparison can be made.
  local index=0
  while [[ ${index} -lt ${count} ]]; do
    local left_element="${left_array[${index}]}"
    local right_element="${right_array[${index}]}"
    if [[ ${left_element} -gt ${right_element} ]]; then
      return 0
    elif [[ ${left_element} -lt ${right_element} ]]; then
      return 1
    fi
    ((++index))
  done

  # If there are more components on the left than on the right, continue
  # comparing, assuming 0 for each of the missing components on the right.
  while [[ ${index} -lt ${left_count} ]]; do
    local left_element="${left_array[${index}]}"
    if [[ ${left_element} -gt 0 ]]; then
      return 0
    fi
    ((++index))
  done

  # If there are more components on the right than on the left, continue
  # comparing, assuming 0 for each of the missing components on the left.
  while [[ ${index} -lt ${right_count} ]]; do
    local right_element="${right_array[${index}]}"
    if [[ ${right_element} -gt 0 ]]; then
      return 1
    fi
    ((++index))
  done

  # Upon reaching this point, the two version numbers are semantically equal.
  return 0
}

# Prints the version of ksadmin, as reported by ksadmin --ksadmin-version, to
# stdout.  This function operates with "static" variables: it will only check
# the ksadmin version once per script run.  If ksadmin is old enough to not
# support --ksadmin-version, or another error occurs, this function prints an
# empty string.
g_checked_ksadmin_version=
g_ksadmin_version=
ksadmin_version() {
  if [[ -z "${g_checked_ksadmin_version}" ]]; then
    g_checked_ksadmin_version="y"
    if [[ -n "${GOOGLE_CHROME_UPDATER_TEST_PATH}" ]]; then
      note "test mode: not calling Keystone, g_ksadmin_version is fake"

      # This isn't very special, it's just what happens to be current as this is
      # written. It's new enough that all of the feature checks
      # (ksadmin_supports_*) pass.
      g_ksadmin_version="1.2.13.41"
    else
      g_ksadmin_version="$(ksadmin --ksadmin-version || true)"
    fi
    note "g_ksadmin_version = ${g_ksadmin_version}"
  fi
  echo "${g_ksadmin_version}"
  return 0
}

# Compares the installed ksadmin version against a supplied version number,
# |check_version|, and returns 0 (true) if the installed Keystone version is
# greater than or equal to |check_version| according to a piece-wise
# comparison.  Returns 1 (false) if the installed Keystone version number
# cannot be determined or if |check_version| is greater than the installed
# Keystone version.  |check_version| should be a string of the form
# "major.minor.micro.build".
is_ksadmin_version_ge() {
  local check_version="${1}"

  local ksadmin_version="$(ksadmin_version)"
  is_version_ge "${ksadmin_version}" "${check_version}"

  # The return value of is_version_ge is used as this function's return value.
}

# Returns 0 (true) if ksadmin supports --tag.
ksadmin_supports_tag() {
  local ksadmin_version

  ksadmin_version="$(ksadmin_version)"
  if [[ -n "${ksadmin_version}" ]]; then
    # A ksadmin that recognizes --ksadmin-version and provides a version
    # number is new enough to recognize --tag.
    return 0
  fi

  return 1
}

# Returns 0 (true) if ksadmin supports --tag-path and --tag-key.
ksadmin_supports_tagpath_tagkey() {
  # --tag-path and --tag-key were introduced in Keystone 1.0.7.1306.
  is_ksadmin_version_ge 1.0.7.1306

  # The return value of is_ksadmin_version_ge is used as this function's
  # return value.
}

# Returns 0 (true) if ksadmin supports --brand-path and --brand-key.
ksadmin_supports_brandpath_brandkey() {
  # --brand-path and --brand-key were introduced in Keystone 1.0.8.1620.
  is_ksadmin_version_ge 1.0.8.1620

  # The return value of is_ksadmin_version_ge is used as this function's
  # return value.
}

# Returns 0 (true) if ksadmin supports --version-path and --version-key.
ksadmin_supports_versionpath_versionkey() {
  # --version-path and --version-key were introduced in Keystone 1.0.9.2318.
  is_ksadmin_version_ge 1.0.9.2318

  # The return value of is_ksadmin_version_ge is used as this function's
  # return value.
}

# Runs "defaults read" to obtain the value of a key in a property list. As
# with "defaults read", an absolute path to a plist is supplied, without the
# ".plist" extension.
#
# As of Mac OS X 10.8, defaults (and NSUserDefaults and CFPreferences)
# normally communicates with cfprefsd to read and write plists. Changes to a
# plist file aren't necessarily reflected immediately via this API family when
# not made through this API family, because cfprefsd may return cached data
# from a former on-disk version of a plist file instead of reading the current
# version from disk. The old behavior can be restored by setting the
# __CFPREFERENCES_AVOID_DAEMON environment variable, although extreme care
# should be used because portions of the system that use this API family
# normally and thus use cfprefsd and its cache will become unsynchronized with
# the on-disk state.
#
# This function is provided to set __CFPREFERENCES_AVOID_DAEMON when calling
# "defaults read" and thus avoid cfprefsd and its on-disk cache, and is
# intended only to be used to read values from Info.plist files, which are not
# preferences. The use of "defaults" for this purpose has always been
# questionable, but there's no better option to interact with plists from
# shell scripts. Definitely don't use infoplist_read to read preference
# plists.
#
# This function exists because the update process delivers new copies of
# Info.plist files to the disk behind cfprefsd's back, and if cfprefsd becomes
# aware of the original version of the file for any reason (such as this
# script reading values from it via "defaults read"), the new version of the
# file will not be immediately effective or visible via cfprefsd after the
# update is applied.
infoplist_read() {
  __CFPREFERENCES_AVOID_DAEMON=1 defaults read "${@}"
}

# When a patch update fails because the old installed copy doesn't match the
# expected state, mark_failed_patch_update updates the Keystone ticket by
# adding "-full" to the tag. The server will see this on a subsequent update
# attempt and will provide a full update (as opposed to a patch) to the
# client.
#
# Even if mark_failed_patch_update fails to modify the tag, the user will
# eventually be updated. Patch updates are only provided for successive
# releases on a particular channel, to update version o to version o+1. If a
# patch update fails in this case, eventually version o+2 will be released,
# and no patch update will exist to update o to o+2, so the server will
# provide a full update package.
mark_failed_patch_update() {
  local product_id="${1}"
  local want_full_installer_path="${2}"
  local old_ks_plist="${3}"
  local old_version_app="${4}"
  local system_ticket="${5}"

  # This step isn't critical.
  local set_e=
  if [[ "${-}" =~ e ]]; then
    set_e="y"
    set +e
  fi

  note "marking failed patch update"

  local channel
  channel="$(infoplist_read "${old_ks_plist}" "${KS_CHANNEL_KEY}" 2> /dev/null)"

  local tag="${channel}"
  local tag_key="${KS_CHANNEL_KEY}"

  tag="${tag}-full"
  tag_key="${tag_key}-full"

  note "tag = ${tag}"
  note "tag_key = ${tag_key}"

  # ${old_ks_plist}, used for --tag-path, is the Info.plist for the old
  # version of Chrome. It may not contain the keys for the "-full" tag suffix.
  # If it doesn't, just bail out without marking the patch update as failed.
  local read_tag="$(infoplist_read "${old_ks_plist}" "${tag_key}" 2> /dev/null)"
  note "read_tag = ${read_tag}"
  if [[ -z "${read_tag}" ]]; then
    note "couldn't mark failed patch update"
    if [[ -n "${set_e}" ]]; then
      set -e
    fi
    return 0
  fi

  # Chrome can't easily read its Keystone ticket prior to registration, and
  # when Chrome registers with Keystone, it obliterates old tag values in its
  # ticket. Therefore, an alternative mechanism is provided to signal to
  # Chrome that a full installer is desired. If the .want_full_installer file
  # is present and it contains Chrome's current version number, Chrome will
  # include "-full" in its tag when it registers with Keystone. This allows
  # "-full" to persist in the tag even after Chrome is relaunched, which on a
  # user ticket, triggers a re-registration.
  #
  # .want_full_installer is placed immediately inside the .app bundle as a
  # sibling to the Contents directory. In this location, it's outside of the
  # view of the code signing and code signature verification machinery. This
  # file can safely be added, modified, and removed without affecting the
  # signature.
  rm -f "${want_full_installer_path}" 2> /dev/null
  echo "${old_version_app}" > "${want_full_installer_path}"

  # See the comment below in the "setting permissions" section for an
  # explanation of the groups and modes selected here.
  local chmod_mode="644"
  if [[ -z "${system_ticket}" ]] &&
     [[ "${want_full_installer_path:0:14}" = "/Applications/" ]] &&
     chgrp admin "${want_full_installer_path}" 2> /dev/null; then
    chmod_mode="664"
  fi
  note "chmod_mode = ${chmod_mode}"
  chmod "${chmod_mode}" "${want_full_installer_path}" 2> /dev/null

  local old_ks_plist_path="${old_ks_plist}.plist"

  # Using ksadmin without --register only updates specified values in the
  # ticket, without changing other existing values.
  local ksadmin_args=(
    --productid "${product_id}"
  )

  if ksadmin_supports_tag; then
    ksadmin_args+=(
      --tag "${tag}"
    )
  fi

  if ksadmin_supports_tagpath_tagkey; then
    ksadmin_args+=(
      --tag-path "${old_ks_plist_path}"
      --tag-key "${tag_key}"
    )
  fi

  note "ksadmin_args = ${ksadmin_args[*]}"

  if [[ -n "${GOOGLE_CHROME_UPDATER_TEST_PATH}" ]]; then
    note "test mode: not calling Keystone to mark failed patch update"
  elif ! ksadmin "${ksadmin_args[@]}"; then
    err "ksadmin failed to mark failed patch update"
  else
    note "marked failed patch update"
  fi

  # Go back to how things were.
  if [[ -n "${set_e}" ]]; then
    set -e
  fi
}

usage() {
  echo "usage: ${ME} update_dmg_mount_point" >& 2
}

main() {
  local update_dmg_mount_point="${1}"

  # Early steps are critical.  Don't continue past any failure.
  set -e

  trap cleanup EXIT HUP INT QUIT TERM

  readonly PRODUCT_NAME="Google Chrome"
  readonly APP_DIR="${PRODUCT_NAME}.app"
  readonly ALTERNATE_APP_DIR="${PRODUCT_NAME} Canary.app"
  readonly FRAMEWORK_NAME="${PRODUCT_NAME} Framework"
  readonly FRAMEWORK_DIR="${FRAMEWORK_NAME}.framework"
  readonly PATCH_DIR=".patch"
  readonly CONTENTS_DIR="Contents"
  readonly APP_PLIST="${CONTENTS_DIR}/Info"
  readonly VERSIONS_DIR_NEW=\
"${CONTENTS_DIR}/Frameworks/${FRAMEWORK_DIR}/Versions"
  readonly VERSIONS_DIR_OLD="${CONTENTS_DIR}/Versions"
  readonly UNROOTED_BRAND_PLIST="Library/Google/Google Chrome Brand"
  readonly UNROOTED_DEBUG_FILE="Library/Google/Google Chrome Updater Debug"
  readonly UNROOTED_KS_BUNDLE_DIR=\
"Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle"

  readonly APP_VERSION_KEY="CFBundleShortVersionString"
  readonly APP_BUNDLEID_KEY="CFBundleIdentifier"
  readonly KS_VERSION_KEY="KSVersion"
  readonly KS_PRODUCT_KEY="KSProductID"
  readonly KS_URL_KEY="KSUpdateURL"
  readonly KS_BRAND_KEY="KSBrandID"

  readonly QUARANTINE_ATTR="com.apple.quarantine"

  # Don't use rsync --archive, because --archive includes --group and --owner,
  # which copy groups and owners, respectively, from the source, and that is
  # undesirable in this case (often, this script will have permission to set
  # those attributes).  --archive also includes --devices and --specials, which
  # copy files that should never occur in the transfer; --devices only works
  # when running as root, so for consistency between privileged and unprivileged
  # operation, this option is omitted as well.  --archive does not include
  # --ignore-times, which is desirable, as it forces rsync to copy files even
  # when their sizes and modification times are identical, as their content
  # still may be different.
  readonly RSYNC_FLAGS="--ignore-times --links --perms --recursive --times"

  # It's difficult to get GOOGLE_CHROME_UPDATER_DEBUG set in the environment
  # when this script is called from Keystone.  If a "debug file" exists in
  # either the root directory or the home directory of the user who owns the
  # ticket, turn on verbosity.  This may aid debugging.
  if [[ -e "/${UNROOTED_DEBUG_FILE}" ]] ||
     [[ -e ~/"${UNROOTED_DEBUG_FILE}" ]]; then
    export GOOGLE_CHROME_UPDATER_DEBUG="y"
  fi

  note "update_dmg_mount_point = ${update_dmg_mount_point}"

  # The argument should be the disk image path.  Make sure it exists and that
  # it's an absolute path.
  note "checking update"

  if [[ -z "${update_dmg_mount_point}" ]] ||
     [[ "${update_dmg_mount_point:0:1}" != "/" ]] ||
     ! [[ -d "${update_dmg_mount_point}" ]]; then
    err "update_dmg_mount_point must be an absolute path to a directory"
    usage
    exit 2
  fi

  local patch_dir="${update_dmg_mount_point}/${PATCH_DIR}"
  if [[ "${patch_dir:0:1}" != "/" ]]; then
    note "patch_dir = ${patch_dir}"
    err "patch_dir must be an absolute path"
    exit 2
  fi

  # Figure out if this is an ordinary installation disk image being used as a
  # full update, or a patch.  A patch will have a .patch directory at the root
  # of the disk image containing information about the update, tools to apply
  # it, and the update contents.
  local is_patch=
  local dirpatcher=
  if [[ -d "${patch_dir}" ]]; then
    # patch_dir exists and is a directory - this is a patch update.
    is_patch="y"
    dirpatcher="${patch_dir}/dirpatcher.sh"
    if ! [[ -x "${dirpatcher}" ]]; then
      err "couldn't locate dirpatcher"
      exit 6
    fi
  elif [[ -e "${patch_dir}" ]]; then
    # patch_dir exists, but is not a directory - what's that mean?
    note "patch_dir = ${patch_dir}"
    err "patch_dir must be a directory"
    exit 2
  else
    # patch_dir does not exist - this is a full "installer."
    patch_dir=
  fi
  note "patch_dir = ${patch_dir}"
  note "is_patch = ${is_patch}"
  note "dirpatcher = ${dirpatcher}"

  # The update to install.

  # update_app is the path to the new version of the .app.  It will only be
  # set at this point for a non-patch update.  It is not yet set for a patch
  # update because no such directory exists yet; it will be set later when
  # dirpatcher creates it.
  local update_app=

  # update_version_app_old, patch_app_dir, and patch_versioned_dir will only
  # be set for patch updates.
  local update_version_app_old=
  local patch_app_dir=
  local patch_versioned_dir=

  local update_version_app update_version_ks product_id update_layout_new
  if [[ -z "${is_patch}" ]]; then
    update_app="${update_dmg_mount_point}/${APP_DIR}"
    note "update_app = ${update_app}"

    # Make sure that it's an absolute path.
    if [[ "${update_app:0:1}" != "/" ]]; then
      err "update_app must be an absolute path"
      exit 2
    fi

    # Make sure there's something to copy from.
    if ! [[ -d "${update_app}" ]]; then
      update_app="${update_dmg_mount_point}/${ALTERNATE_APP_DIR}"
      note "update_app = ${update_app}"

      if [[ "${update_app:0:1}" != "/" ]]; then
        err "update_app (alternate) must be an absolute path"
        exit 2
      fi

      if ! [[ -d "${update_app}" ]]; then
        err "update_app must be a directory"
        exit 2
      fi
    fi

    # Get some information about the update.
    note "reading update values"

    local update_app_plist="${update_app}/${APP_PLIST}"
    note "update_app_plist = ${update_app_plist}"
    if ! update_version_app="$(infoplist_read "${update_app_plist}" \
                                              "${APP_VERSION_KEY}")" ||
       [[ -z "${update_version_app}" ]]; then
      err "couldn't determine update_version_app"
      exit 2
    fi
    note "update_version_app = ${update_version_app}"

    local update_ks_plist="${update_app_plist}"
    note "update_ks_plist = ${update_ks_plist}"
    if ! update_version_ks="$(infoplist_read "${update_ks_plist}" \
                                             "${KS_VERSION_KEY}")" ||
       [[ -z "${update_version_ks}" ]]; then
      err "couldn't determine update_version_ks"
      exit 2
    fi
    note "update_version_ks = ${update_version_ks}"

    if ! product_id="$(infoplist_read "${update_ks_plist}" \
                                      "${KS_PRODUCT_KEY}")" ||
       [[ -z "${product_id}" ]]; then
      err "couldn't determine product_id"
      exit 2
    fi
    note "product_id = ${product_id}"

    if [[ -d "${update_app}/${VERSIONS_DIR_NEW}" ]]; then
      update_layout_new="y"
    fi
    note "update_layout_new = ${update_layout_new}"
  else  # [[ -n "${is_patch}" ]]
    # Get some information about the update.
    note "reading update values"

    if ! update_version_app_old=$(<"${patch_dir}/old_app_version") ||
       [[ -z "${update_version_app_old}" ]]; then
      err "couldn't determine update_version_app_old"
      exit 2
    fi
    note "update_version_app_old = ${update_version_app_old}"

    if ! update_version_app=$(<"${patch_dir}/new_app_version") ||
       [[ -z "${update_version_app}" ]]; then
      err "couldn't determine update_version_app"
      exit 2
    fi
    note "update_version_app = ${update_version_app}"

    if ! update_version_ks=$(<"${patch_dir}/new_ks_version") ||
       [[ -z "${update_version_ks}" ]]; then
      err "couldn't determine update_version_ks"
      exit 2
    fi
    note "update_version_ks = ${update_version_ks}"

    if ! product_id=$(<"${patch_dir}/ks_product") ||
       [[ -z "${product_id}" ]]; then
      err "couldn't determine product_id"
      exit 2
    fi
    note "product_id = ${product_id}"

    patch_app_dir="${patch_dir}/application.dirpatch"
    if ! [[ -d "${patch_app_dir}" ]]; then
      err "couldn't locate patch_app_dir"
      exit 6
    fi
    note "patch_app_dir = ${patch_app_dir}"

    patch_versioned_dir="${patch_dir}/\
framework_${update_version_app_old}_${update_version_app}.dirpatch"
    if [[ -d "${patch_versioned_dir}" ]]; then
      update_layout_new="y"
    else
      patch_versioned_dir=\
"${patch_dir}/version_${update_version_app_old}_${update_version_app}.dirpatch"
      if ! [[ -d "${patch_versioned_dir}" ]]; then
        err "couldn't locate patch_versioned_dir"
        exit 6
      fi
    fi
    note "patch_versioned_dir = ${patch_versioned_dir}"
    note "update_layout_new = ${update_layout_new}"
  fi

  # ksadmin is required. Keystone should have set a ${PATH} that includes it.
  # Check that here, so that more useful feedback can be offered in the
  # unlikely event that ksadmin is missing.
  note "checking Keystone"

  if [[ -n "${GOOGLE_CHROME_UPDATER_TEST_PATH}" ]]; then
    note "test mode: not setting ksadmin_path"
  else
    local ksadmin_path
    if ! ksadmin_path="$(type -p ksadmin)" || [[ -z "${ksadmin_path}" ]]; then
      err "couldn't locate ksadmin_path"
      exit 3
    fi
    note "ksadmin_path = ${ksadmin_path}"
  fi

  # Call ksadmin_version once to prime the global state.  This is needed
  # because subsequent calls to ksadmin_version that occur in $(...)
  # expansions will not affect the global state (although they can read from
  # the already-initialized global state) and thus will cause a new ksadmin
  # --ksadmin-version process to run for each check unless the globals have
  # been properly initialized beforehand.
  ksadmin_version >& /dev/null || true
  local ksadmin_version_string
  ksadmin_version_string="$(ksadmin_version 2> /dev/null || true)"
  note "ksadmin_version_string = ${ksadmin_version_string}"

  # Figure out where to install.
  local installed_app
  if [[ -n "${GOOGLE_CHROME_UPDATER_TEST_PATH}" ]]; then
    note "test mode: not calling Keystone, installed_app is from environment"
    installed_app="${GOOGLE_CHROME_UPDATER_TEST_PATH}"
  elif ! installed_app="$(ksadmin -pP "${product_id}" | sed -Ene \
      "s%^[[:space:]]+xc=<KSPathExistenceChecker:.* path=(/.+)>\$%\\1%p")" ||
      [[ -z "${installed_app}" ]]; then
    err "couldn't locate installed_app"
    exit 3
  fi
  note "installed_app = ${installed_app}"

  local want_full_installer_path="${installed_app}/.want_full_installer"
  note "want_full_installer_path = ${want_full_installer_path}"

  if [[ "${installed_app:0:1}" != "/" ]] ||
     ! [[ -d "${installed_app}" ]]; then
    err "installed_app must be an absolute path to a directory"
    exit 3
  fi

  # If this script is running as root, it's being driven by a system ticket.
  # Otherwise, it's being driven by a user ticket.
  local system_ticket=
  if [[ ${EUID} -eq 0 ]]; then
    system_ticket="y"
  fi
  note "system_ticket = ${system_ticket}"

  # If this script is being driven by a user ticket, but a system ticket is also
  # present and system Keystone is installed, there's a potential for the two
  # tickets to collide.  Both ticket types might be present if another user on
  # the system promoted the ticket to system: the other user could not have
  # removed this user's user ticket.  Handle that case here by deleting the user
  # ticket and exiting early with a discrete exit code.
  #
  # Current versions of ksadmin will exit 1 (false) when asked to print tickets
  # and given a specific product ID to print.  Older versions of ksadmin would
  # exit 0 (true), but those same versions did not support -S (meaning to check
  # the system ticket store) and would exit 1 (false) with this invocation due
  # to not understanding the question.  Therefore, the usage here will only
  # delete the existing user ticket when running as non-root with access to a
  # sufficiently recent ksadmin.  Older ksadmins are tolerated: the update will
  # likely fail for another reason and the user ticket will hang around until
  # something is eventually able to remove it.
  if [[ -z "${GOOGLE_CHROME_UPDATER_TEST_PATH}" ]] &&
     [[ -z "${system_ticket}" ]] &&
     [[ -d "/${UNROOTED_KS_BUNDLE_DIR}" ]] &&
     ksadmin -S --print-tickets --productid "${product_id}" >& /dev/null; then
    ksadmin --delete --productid "${product_id}" || true
    err "can't update on a user ticket when a system ticket is also present"
    exit 4
  fi

  # Figure out what the existing installed application is using for its
  # versioned directory.  This will be used later, to avoid removing the
  # existing installed version's versioned directory in case anything is still
  # using it.
  note "reading install values"

  local installed_app_plist="${installed_app}/${APP_PLIST}"
  note "installed_app_plist = ${installed_app_plist}"
  local installed_app_plist_path="${installed_app_plist}.plist"
  note "installed_app_plist_path = ${installed_app_plist_path}"
  local old_version_app
  old_version_app="$(infoplist_read "${installed_app_plist}" \
                                    "${APP_VERSION_KEY}" || true)"
  note "old_version_app = ${old_version_app}"

  # old_version_app is not required, because it won't be present in skeleton
  # bootstrap installations, which just have an empty .app directory.  Only
  # require it when doing a patch update, and use it to validate that the
  # patch applies to the old installed version.  By definition, skeleton
  # bootstraps can't be installed with patch updates.  They require the full
  # application on the disk image.
  if [[ -n "${is_patch}" ]]; then
    if [[ -z "${old_version_app}" ]]; then
      err "old_version_app required for patch"
      exit 6
    elif [[ "${old_version_app}" != "${update_version_app_old}" ]]; then
      err "this patch does not apply to the installed version"
      exit 6
    fi
  fi

  local installed_versions_dir_new="${installed_app}/${VERSIONS_DIR_NEW}"
  note "installed_versions_dir_new = ${installed_versions_dir_new}"
  local installed_versions_dir_old="${installed_app}/${VERSIONS_DIR_OLD}"
  note "installed_versions_dir_old = ${installed_versions_dir_old}"

  local installed_versions_dir
  if [[ -n "${update_layout_new}" ]]; then
    installed_versions_dir="${installed_versions_dir_new}"
  else
    installed_versions_dir="${installed_versions_dir_old}"
  fi
  note "installed_versions_dir = ${installed_versions_dir}"

  # If the installed application is incredibly old, or in a skeleton bootstrap
  # installation, old_versioned_dir may not exist.
  local old_versioned_dir
  if [[ -n "${old_version_app}" ]]; then
    if [[ -d "${installed_versions_dir_new}/${old_version_app}" ]]; then
      old_versioned_dir="${installed_versions_dir_new}/${old_version_app}"
    elif [[ -d "${installed_versions_dir_old}/${old_version_app}" ]]; then
      old_versioned_dir="${installed_versions_dir_old}/${old_version_app}"
    fi
  fi
  note "old_versioned_dir = ${old_versioned_dir}"

  # Collect the installed application's brand code, it will be used later.  It
  # is not an error for the installed application to not have a brand code.
  local old_ks_plist="${installed_app_plist}"
  note "old_ks_plist = ${old_ks_plist}"
  local old_brand
  old_brand="$(infoplist_read "${old_ks_plist}" \
                              "${KS_BRAND_KEY}" 2> /dev/null ||
               true)"
  note "old_brand = ${old_brand}"

  local update_versioned_dir=
  if [[ -z "${is_patch}" ]]; then
    if [[ -n "${update_layout_new}" ]]; then
      update_versioned_dir=\
"${update_app}/${VERSIONS_DIR_NEW}/${update_version_app}"
    else
      update_versioned_dir=\
"${update_app}/${VERSIONS_DIR_OLD}/${update_version_app}"
    fi
    note "update_versioned_dir = ${update_versioned_dir}"
  fi

  ensure_writable_symlinks_recursive "${installed_app}"

  # By copying to ${installed_app}, the existing application name will be
  # preserved, if the user has renamed the application on disk.  Respecting
  # the user's changes is friendly.

  # Make sure that ${installed_versions_dir} exists, so that it can receive
  # the versioned directory.  It may not exist if updating from an older
  # version that did not use the same versioned layout on disk.  Later, during
  # the rsync to copy the application directory, the mode bits and timestamp on
  # ${installed_versions_dir} will be set to conform to whatever is present in
  # the update.
  #
  # ${installed_app} is guaranteed to exist at this point, but
  # ${installed_app}/${CONTENTS_DIR} may not if things are severely broken or
  # if this update is actually an initial installation from a Keystone
  # skeleton bootstrap.  The mkdir creates ${installed_app}/${CONTENTS_DIR} if
  # it doesn't exist; its mode bits will be fixed up in a subsequent rsync.
  note "creating installed_versions_dir"
  if ! mkdir -p "${installed_versions_dir}"; then
    err "mkdir of installed_versions_dir failed"
    exit 5
  fi

  local new_versioned_dir
  new_versioned_dir="${installed_versions_dir}/${update_version_app}"
  note "new_versioned_dir = ${new_versioned_dir}"

  # If there's an entry at ${new_versioned_dir} but it's not a directory
  # (or it's a symbolic link, whether or not it points to a directory), rsync
  # won't get rid of it.  It's never correct to have a non-directory in place
  # of the versioned directory, so toss out whatever's there.  Don't treat
  # this as a critical step: if removal fails, operation can still proceed to
  # to the dirpatcher or rsync, which will likely fail.
  if [[ -e "${new_versioned_dir}" ]] &&
     ([[ -L "${new_versioned_dir}" ]] ||
      ! [[ -d "${new_versioned_dir}" ]]); then
    note "removing non-directory in place of versioned directory"
    rm -f "${new_versioned_dir}" 2> /dev/null || true
  fi

  if [[ -n "${is_patch}" ]]; then
    # dirpatcher won't patch into a directory that already exists.  Doing so
    # would be a bad idea, anyway.  If ${new_versioned_dir} already exists,
    # it may be something left over from a previous failed or incomplete
    # update attempt, or it may be the live versioned directory if this is a
    # same-version update intended only to change channels.  Since there's no
    # way to tell, this case is handled by having dirpatcher produce the new
    # versioned directory in a temporary location and then having rsync copy
    # it into place as an ${update_versioned_dir}, the same as in a non-patch
    # update.  If ${new_versioned_dir} doesn't exist, dirpatcher can place the
    # new versioned directory at that location directly.
    local versioned_dir_target
    if ! [[ -e "${new_versioned_dir}" ]]; then
      versioned_dir_target="${new_versioned_dir}"
      note "versioned_dir_target = ${versioned_dir_target}"
    else
      ensure_temp_dir
      versioned_dir_target="${g_temp_dir}/${update_version_app}"
      note "versioned_dir_target = ${versioned_dir_target}"
      update_versioned_dir="${versioned_dir_target}"
      note "update_versioned_dir = ${update_versioned_dir}"
    fi

    note "dirpatching versioned directory"
    if ! "${dirpatcher}" "${old_versioned_dir}" \
                         "${patch_versioned_dir}" \
                         "${versioned_dir_target}"; then
      err "dirpatcher of versioned directory failed, status ${PIPESTATUS[0]}"
      mark_failed_patch_update "${product_id}" \
                               "${want_full_installer_path}" \
                               "${old_ks_plist}" \
                               "${old_version_app}" \
                               "${system_ticket}"

      if [[ -n "${update_layout_new}" ]] &&
         [[ "${versioned_dir_target}" = "${new_versioned_dir}" ]]; then
        # If the dirpatcher of a new-layout versioned directory failed while
        # writing directly to the target location, remove it. The incomplete
        # version would break code signature validation under the new layout.
        # If it was being staged in a temporary directory, there's nothing to
        # clean up beyond cleaning up the temporary directory, which will happen
        # normally at exit.
        note "cleaning up new_versioned_dir"
        rm -rf "${new_versioned_dir}"
      fi

      exit 12
    fi
  fi

  # Copy the versioned directory.  The new versioned directory should have a
  # different name than any existing one, so this won't harm anything already
  # present in ${installed_versions_dir}, including the versioned directory
  # being used by any running processes.  If this step is interrupted, there
  # will be an incomplete versioned directory left behind, but it won't
  # won't interfere with anything, and it will be replaced or removed during a
  # future update attempt.
  #
  # In certain cases, same-version updates are distributed to move users
  # between channels; when this happens, the contents of the versioned
  # directories are identical and rsync will not render the versioned
  # directory unusable even for an instant.
  #
  # ${update_versioned_dir} may be empty during a patch update (${is_patch})
  # if the dirpatcher above was able to write it into place directly.  In
  # that event, dirpatcher guarantees that ${new_versioned_dir} is already in
  # place.
  if [[ -n "${update_versioned_dir}" ]]; then
    note "rsyncing versioned directory"
    if ! rsync ${RSYNC_FLAGS} --delete-before "${update_versioned_dir}/" \
                                              "${new_versioned_dir}"; then
      err "rsync of versioned directory failed, status ${PIPESTATUS[0]}"

      if [[ -n "${update_layout_new}" ]]; then
        # If the rsync of a new-layout versioned directory failed, remove it.
        # The incomplete version would break code signature validation.
        note "cleaning up new_versioned_dir"
        rm -rf "${new_versioned_dir}"
      fi

      exit 7
    fi
  fi

  if [[ -n "${is_patch}" ]]; then
    # If the versioned directory was prepared in a temporary directory and
    # then rsynced into place, remove the temporary copy now that it's no
    # longer needed.
    if [[ -n "${update_versioned_dir}" ]]; then
      rm -rf "${update_versioned_dir}" 2> /dev/null || true
      update_versioned_dir=
      note "update_versioned_dir = ${update_versioned_dir}"
    fi

    # Prepare ${update_app}.  This always needs to be done in a temporary
    # location because dirpatcher won't write to a directory that already
    # exists, and ${installed_app} needs to be used as input to dirpatcher
    # in any event.  The new application will be rsynced into place once
    # dirpatcher creates it.
    ensure_temp_dir
    update_app="${g_temp_dir}/${APP_DIR}"
    note "update_app = ${update_app}"

    note "dirpatching app directory"
    if ! "${dirpatcher}" "${installed_app}" \
                         "${patch_app_dir}" \
                         "${update_app}"; then
      err "dirpatcher of app directory failed, status ${PIPESTATUS[0]}"
      mark_failed_patch_update "${product_id}" \
                               "${want_full_installer_path}" \
                               "${old_ks_plist}" \
                               "${old_version_app}" \
                               "${system_ticket}"
      exit 13
    fi
  fi

  # See if the timestamp of what's currently on disk is newer than the
  # update's outer .app's timestamp.  rsync will copy the update's timestamp
  # over, but if that timestamp isn't as recent as what's already on disk, the
  # .app will need to be touched.
  local needs_touch=
  if [[ "${installed_app}" -nt "${update_app}" ]]; then
    needs_touch="y"
  fi
  note "needs_touch = ${needs_touch}"

  # Copy the unversioned files into place, leaving everything in
  # ${installed_versions_dir} alone.  If this step is interrupted, the
  # application will at least remain in a usable state, although it may not
  # pass signature validation.  Depending on when this step is interrupted,
  # the application will either launch the old or the new version.  The
  # critical point is when the main executable is replaced.  There isn't very
  # much to copy in this step, because most of the application is in the
  # versioned directory.  This step only accounts for around 50 files, most of
  # which are small localized InfoPlist.strings files.  Note that
  # ${VERSIONS_DIR_NEW} or ${VERSIONS_DIR_OLD} are included to copy their mode
  # bits and timestamps, but their contents are excluded, having already been
  # installed above. The ${VERSIONS_DIR_NEW}/Current symbolic link is updated
  # or created in this step, however.
  note "rsyncing app directory"
  if ! rsync ${RSYNC_FLAGS} --delete-after \
       --include="/${VERSIONS_DIR_NEW}/Current" \
       --exclude="/${VERSIONS_DIR_NEW}/*" --exclude="/${VERSIONS_DIR_OLD}/*" \
       "${update_app}/" "${installed_app}"; then
    err "rsync of app directory failed, status ${PIPESTATUS[0]}"
    exit 8
  fi

  note "rsyncs complete"

  if [[ -n "${is_patch}" ]]; then
    # update_app has been rsynced into place and is no longer needed.
    rm -rf "${update_app}" 2> /dev/null || true
    update_app=
    note "update_app = ${update_app}"
  fi

  if [[ -n "${g_temp_dir}" ]]; then
    # The temporary directory, if any, is no longer needed.
    rm -rf "${g_temp_dir}" 2> /dev/null || true
    g_temp_dir=
    note "g_temp_dir = ${g_temp_dir}"
  fi

  # Clean up any old .want_full_installer files from previous dirpatcher
  # failures. This is not considered a critical step, because this file
  # normally does not exist at all.
  rm -f "${want_full_installer_path}" || true

  # If necessary, touch the outermost .app so that it appears to the outside
  # world that something was done to the bundle.  This will cause
  # LaunchServices to invalidate the information it has cached about the
  # bundle even if lsregister does not run.  This is not done if rsync already
  # updated the timestamp to something newer than what had been on disk.  This
  # is not considered a critical step, and if it fails, this script will not
  # exit.
  if [[ -n "${needs_touch}" ]]; then
    touch -cf "${installed_app}" || true
  fi

  # Read the new values, such as the version.
  note "reading new values"

  local new_version_app
  if ! new_version_app="$(infoplist_read "${installed_app_plist}" \
                                         "${APP_VERSION_KEY}")" ||
     [[ -z "${new_version_app}" ]]; then
    err "couldn't determine new_version_app"
    exit 9
  fi
  note "new_version_app = ${new_version_app}"

  local new_versioned_dir="${installed_versions_dir}/${new_version_app}"
  note "new_versioned_dir = ${new_versioned_dir}"

  local new_ks_plist="${installed_app_plist}"
  note "new_ks_plist = ${new_ks_plist}"

  local new_version_ks
  if ! new_version_ks="$(infoplist_read "${new_ks_plist}" \
                                        "${KS_VERSION_KEY}")" ||
     [[ -z "${new_version_ks}" ]]; then
    err "couldn't determine new_version_ks"
    exit 9
  fi
  note "new_version_ks = ${new_version_ks}"

  local update_url
  if ! update_url="$(infoplist_read "${new_ks_plist}" "${KS_URL_KEY}")" ||
     [[ -z "${update_url}" ]]; then
    err "couldn't determine update_url"
    exit 9
  fi
  note "update_url = ${update_url}"

  # The channel ID is optional.  Suppress stderr to prevent Keystone from
  # seeing possible error output.
  local channel
  channel="$(infoplist_read "${new_ks_plist}" \
                            "${KS_CHANNEL_KEY}" 2> /dev/null || true)"
  note "channel = ${channel}"

  local tag="${channel}"
  local tag_key="${KS_CHANNEL_KEY}"
  note "tag = ${tag}"
  note "tag_key = ${tag_key}"

  # Make sure that the update was successful by comparing the version found in
  # the update with the version now on disk.
  if [[ "${new_version_ks}" != "${update_version_ks}" ]]; then
    err "new_version_ks and update_version_ks do not match"
    exit 10
  fi

  # Notify LaunchServices.  This is not considered a critical step, and
  # lsregister's exit codes shouldn't be confused with this script's own.
  # Redirect stdout to /dev/null to suppress the useless "ThrottleProcessIO:
  # throttling disk i/o" messages that lsregister might print.
  note "notifying LaunchServices"
  local coreservices="/System/Library/Frameworks/CoreServices.framework"
  local launchservices="${coreservices}/Frameworks/LaunchServices.framework"
  local lsregister="${launchservices}/Support/lsregister"
  note "coreservices = ${coreservices}"
  note "launchservices = ${launchservices}"
  note "lsregister = ${lsregister}"
  "${lsregister}" -f "${installed_app}" > /dev/null || true

  # The brand information is stored differently depending on whether this is
  # running for a system or user ticket.
  note "handling brand code"

  local set_brand_file_access=
  local brand_plist
  if [[ -n "${system_ticket}" ]]; then
    # System ticket.
    set_brand_file_access="y"
    brand_plist="/${UNROOTED_BRAND_PLIST}"
  else
    # User ticket.
    brand_plist=~/"${UNROOTED_BRAND_PLIST}"
  fi
  local brand_plist_path="${brand_plist}.plist"
  note "set_brand_file_access = ${set_brand_file_access}"
  note "brand_plist = ${brand_plist}"
  note "brand_plist_path = ${brand_plist_path}"

  local ksadmin_brand_plist_path
  local ksadmin_brand_key

  # Only the stable channel, identified by an empty channel string, has a
  # brand code. On the beta and dev channels, remove the brand plist if
  # present. Its presence means that the ticket used to manage a
  # stable-channel Chrome but the user has since replaced it with a beta or
  # dev channel version. Since the canary channel can run side-by-side with
  # another Chrome installation, don't remove the brand plist on that channel,
  # but skip the rest of the brand logic.
  if [[ "${channel}" = "beta" ]] || [[ "${channel}" = "dev" ]]; then
    note "defeating brand code on channel ${channel}"
    rm -f "${brand_plist_path}" 2>/dev/null || true
  elif [[ -n "${channel}" ]]; then
    # Canary channel.
    note "skipping brand code on channel ${channel}"
  else
    # Stable channel.
    # If the user manually updated their copy of Chrome, there might be new
    # brand information in the app bundle, and that needs to be copied out
    # into the file Keystone looks at.
    if [[ -n "${old_brand}" ]]; then
      local brand_dir
      brand_dir="$(dirname "${brand_plist_path}")"
      note "brand_dir = ${brand_dir}"
      if ! mkdir -p "${brand_dir}"; then
        err "couldn't mkdir brand_dir, continuing"
      else
        if ! defaults write "${brand_plist}" "${KS_BRAND_KEY}" \
                            -string "${old_brand}"; then
          err "couldn't write brand_plist, continuing"
        elif [[ -n "${set_brand_file_access}" ]]; then
          if ! chown "root:wheel" "${brand_plist_path}"; then
            err "couldn't chown brand_plist_path, continuing"
          else
            if ! chmod 644 "${brand_plist_path}"; then
              err "couldn't chmod brand_plist_path, continuing"
            fi
          fi
        fi
      fi
    fi

    # Confirm that the brand file exists.  It's optional.
    ksadmin_brand_plist_path="${brand_plist_path}"
    ksadmin_brand_key="${KS_BRAND_KEY}"

    if ! [[ -f "${ksadmin_brand_plist_path}" ]]; then
      # Clear any branding information.
      ksadmin_brand_plist_path=
      ksadmin_brand_key=
    fi
  fi

  note "ksadmin_brand_plist_path = ${ksadmin_brand_plist_path}"
  note "ksadmin_brand_key = ${ksadmin_brand_key}"

  note "notifying Keystone"

  local ksadmin_args=(
    --register
    --productid "${product_id}"
    --version "${new_version_ks}"
    --xcpath "${installed_app}"
    --url "${update_url}"
  )

  if ksadmin_supports_tag; then
    ksadmin_args+=(
      --tag "${tag}"
    )
  fi

  if ksadmin_supports_tagpath_tagkey; then
    ksadmin_args+=(
      --tag-path "${installed_app_plist_path}"
      --tag-key "${tag_key}"
    )
  fi

  if ksadmin_supports_brandpath_brandkey; then
    ksadmin_args+=(
      --brand-path "${ksadmin_brand_plist_path}"
      --brand-key "${ksadmin_brand_key}"
    )
  fi

  if ksadmin_supports_versionpath_versionkey; then
    ksadmin_args+=(
      --version-path "${installed_app_plist_path}"
      --version-key "${KS_VERSION_KEY}"
    )
  fi

  note "ksadmin_args = ${ksadmin_args[*]}"

  if [[ -n "${GOOGLE_CHROME_UPDATER_TEST_PATH}" ]]; then
    note "test mode: not calling Keystone to update ticket"
  elif ! ksadmin "${ksadmin_args[@]}"; then
    err "ksadmin failed"
    exit 11
  fi

  # The remaining steps are not considered critical.
  set +e

  # Try to clean up old versions that are not in use.  The strategy is to keep
  # the versioned directory corresponding to the update just applied
  # (obviously) and the version that was just replaced, and to use ps and lsof
  # to see if it looks like any processes are currently using any other old
  # directories.  Directories not in use are removed.  Old versioned
  # directories that are in use are left alone so as to not interfere with
  # running processes.  These directories can be cleaned up by this script on
  # future updates.
  #
  # To determine which directories are in use, both ps and lsof are used.
  # Each approach has limitations.
  #
  # The ps check looks for processes within the versioned directory.  Only
  # helper processes, such as renderers, are within the versioned directory.
  # Browser processes are not, so the ps check will not find them, and will
  # assume that a versioned directory is not in use if a browser is open
  # without any windows.  The ps mechanism can also only detect processes
  # running on the system that is performing the update.  If network shares
  # are involved, all bets are off.
  #
  # The lsof check looks to see what processes have the framework dylib open.
  # Browser processes will have their versioned framework dylib open, so this
  # check is able to catch browsers even if there are no associated helper
  # processes.  Like the ps check, the lsof check is limited to processes on
  # the system that is performing the update.  Finally, unless running as
  # root, the lsof check can only find processes running as the effective user
  # performing the update.
  #
  # These limitations are motivations to additionally preserve the versioned
  # directory corresponding to the version that was just replaced.
  note "cleaning up old versioned directories"

  local versioned_dir
  for versioned_dir in "${installed_versions_dir_new}/"* \
                       "${installed_versions_dir_old}/"*; do
    note "versioned_dir = ${versioned_dir}"
    if [[ "${versioned_dir}" = "${new_versioned_dir}" ]] ||
       [[ "${versioned_dir}" = "${old_versioned_dir}" ]] ||
       [[ "${versioned_dir}" = "${installed_versions_dir_new}/Current" ]]; then
      # This is the versioned directory corresponding to the update that was
      # just applied or the version that was previously in use.  Leave it
      # alone.
      note "versioned_dir is new_versioned_dir or old_versioned_dir, skipping"
      continue
    fi

    # Look for any processes whose executables are within this versioned
    # directory.  They'll be helper processes, such as renderers.  Their
    # existence indicates that this versioned directory is currently in use.
    local ps_string="${versioned_dir}/"
    note "ps_string = ${ps_string}"

    # Look for any processes using the framework dylib.  This will catch
    # browser processes where the ps check will not, but it is limited to
    # processes running as the effective user.
    local lsof_file
    if [[ -e "${versioned_dir}/${FRAMEWORK_DIR}/${FRAMEWORK_NAME}" ]]; then
      # Old layout.
      lsof_file="${versioned_dir}/${FRAMEWORK_DIR}/${FRAMEWORK_NAME}"
    else
      # New layout.
      lsof_file="${versioned_dir}/${FRAMEWORK_NAME}"
    fi
    note "lsof_file = ${lsof_file}"

    # ps -e displays all users' processes, -ww causes ps to not truncate
    # lines, -o comm instructs it to only print the command name, and the =
    # tells it to not print a header line.
    # The cut invocation filters the ps output to only have at most the number
    # of characters in ${ps_string}.  This is done so that grep can look for
    # an exact match.
    # grep -F tells grep to look for lines that are exact matches (not regular
    # expressions), -q tells it to not print any output and just indicate
    # matches by exit status, and -x tells it that the entire line must match
    # ${ps_string} exactly, as opposed to matching a substring.  A match
    # causes grep to exit zero (true).
    #
    # lsof will exit nonzero if ${lsof_file} does not exist or is open by any
    # process.  If the file exists and is open, it will exit zero (true).
    if (! ps -ewwo comm= | \
          cut -c "1-${#ps_string}" | \
          grep -Fqx "${ps_string}") &&
       (! lsof "${lsof_file}" >& /dev/null); then
      # It doesn't look like anything is using this versioned directory.  Get
      # rid of it.
      note "versioned_dir doesn't appear to be in use, removing"
      rm -rf "${versioned_dir}"
    else
      note "versioned_dir is in use, skipping"
    fi
  done

  # When the last old-layout version is gone, remove the old-layout Versions
  # directory. Note that this isn't attempted when the last new-layout Versions
  # directory disappears, because hopefully there won't ever be an "upgrade" (at
  # least not long-term) that needs to revert from the new to the old layout. If
  # this does become necessary, the rmdir should attempt to remove, from
  # innermost to outermost, ${installed_versions_dir_new} out to
  # ${installed_app}/${CONTENTS_DIR}/Frameworks. Even though that removal isn't
  # attempted here, a subsequent update will do this cleanup as a side effect of
  # the outer app rsync, which will remove these directories if empty when
  # "updating" to another old-layout version.
  if [[ -n "${update_layout_new}" ]] &&
     [[ -d "${installed_versions_dir_old}" ]]; then
    note "attempting removal of installed_versions_dir_old"
    rmdir "${installed_versions_dir_old}" >& /dev/null
    if [[ -d "${installed_versions_dir_old}" ]]; then
      note "removal of installed_versions_dir_old failed"
    else
      note "removal of installed_versions_dir_old succeeded"
    fi
  fi

  # If this script is being driven by a user Keystone ticket, it is not
  # running as root.  If the application is installed somewhere under
  # /Applications, try to make it writable by all admin users.  This will
  # allow other admin users to update the application from their own user
  # Keystone instances.
  #
  # If the script is being driven by a user Keystone ticket (not running as
  # root) and the application is not installed under /Applications, it might
  # not be in a system-wide location, and it probably won't be something that
  # other users on the system are running, so err on the side of safety and
  # don't make it group-writable.
  #
  # If this script is being driven by a system ticket (running as root), it's
  # future updates can be expected to be applied the same way, so admin-
  # writability is not a concern.  Set the entire thing to be owned by root
  # in that case, regardless of where it's installed, and drop any group and
  # other write permission.
  #
  # If this script is running as a user that is not a member of the admin
  # group, the chgrp operation will not succeed.  Tolerate that case, because
  # it's better than the alternative, which is to make the application
  # world-writable.
  note "setting permissions"

  local chmod_mode="a+rX,u+w,go-w"
  if [[ -z "${system_ticket}" ]]; then
    if [[ "${installed_app:0:14}" = "/Applications/" ]] &&
       chgrp -Rh admin "${installed_app}" 2> /dev/null; then
      chmod_mode="a+rX,ug+w,o-w"
    fi
  else
    chown -Rh root:wheel "${installed_app}" 2> /dev/null
  fi

  note "chmod_mode = ${chmod_mode}"
  chmod -R "${chmod_mode}" "${installed_app}" 2> /dev/null

  # On the Mac, or at least on HFS+, symbolic link permissions are significant,
  # but chmod -R and -h can't be used together.  Do another pass to fix the
  # permissions on any symbolic links.
  find "${installed_app}" -type l -exec chmod -h "${chmod_mode}" {} + \
      2> /dev/null

  # If an update is triggered from within the application itself, the update
  # process inherits the quarantine bit (LSFileQuarantineEnabled).  Any files
  # or directories created during the update will be quarantined in that case,
  # which may cause Launch Services to display quarantine UI.  That's bad,
  # especially if it happens when the outer .app launches a quarantined inner
  # helper.  If the application is already on the system and is being updated,
  # then it can be assumed that it should not be quarantined.  Use xattr to
  # drop the quarantine attribute.
  #
  # TODO(mark): Instead of letting the quarantine attribute be set and then
  # dropping it here, figure out a way to get the update process to run
  # without LSFileQuarantineEnabled even when triggering an update from within
  # the application.
  note "lifting quarantine"

  xattr -d -r "${QUARANTINE_ATTR}" "${installed_app}" 2> /dev/null

  # Great success!
  note "done!"

  trap - EXIT

  return 0
}

# Check "less than" instead of "not equal to" in case Keystone ever changes to
# pass more arguments.
if [[ ${#} -lt 1 ]]; then
  usage
  exit 2
fi

main "${@}"
exit ${?}
