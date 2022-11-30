#!/bin/bash -p
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# usage: install.sh update_dmg_mount_point installed_app_path current_version
#
# Called by Omaha v4 to update the installed application with a new version from
# the dmg.
#
# Exit codes:
#   0   Success!
#   1   Unknown Failure
#   2   Could not locate installed app path.
#   3   DMG mount point is not an absolute path.
#   4   Path to new .app must be an absolute path.
#   5   Update app is not using the new version folder layout.
#   6   Installed app path must be an absolute path to a directory.
#   7   Installed app's versioned directory is in old format.
#   8   Installed app's versioned directory is in old format.
#   9   Installed app's versioned directory is in old format.
#   10  Making versioned directory for new version failed.
#   11  Could not remove existing file where versioned directory should be.
#   12  rsync of versioned directory failed.
#   13  rsync of app directory failed.
#   14  We could not determine the new app version.
#   15  The new app version does not match the update version.
#   16  This will return a usage message.

set -eu

# Set path to /bin, /usr/bin, /sbin, /usr/sbin
export PATH="/bin:/usr/bin:/sbin:/usr/sbin"

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

# We will populate this variable with the version of the application bundle
# it'll be packaged with when the build is run. This allows us to not do a
# defaults read on Info.plist to find the update_version.
UPDATE_VERSION=
readonly UPDATE_VERSION

err() {
  local error="${1}"
  local id=": ${$} $(date "+%Y-%m-%d %H:%M:%S %z")"
  echo "${ME}${id}: ${error}" >& 2
}

note() {
  local message="${1}"
  echo "${ME}: ${$} $(date "+%Y-%m-%d %H:%M:%S %z"): ${message}" >& 1
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

# Returns 0 (true) if |symlink| exists, is a symbolic link, and appears
# writable on the basis of its POSIX permissions.  This is used to determine
# writability like test's -w primary, but -w resolves symbolic links and this
# function does not.
is_writable_symlink() {
  local symlink="${1}"

  local link_mode="$(stat -f %Sp "${symlink}" 2> /dev/null || true)"
  if [[ -z "${link_mode}" ]] || [[ "${link_mode:0:1}" != "l" ]]; then
    return 1
  fi

  local link_user="$(stat -f %u "${symlink}" 2> /dev/null || true)"
  local link_group="$(stat -f %g "${symlink}" 2> /dev/null || true)"
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

    local target="$(readlink "${symlink}" 2> /dev/null || true)"
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

    local symlink_dir="$(dirname "${symlink}")"
    local temp_link_dir=\
      "$(mktemp -d "${symlink_dir}/.symlink_temp.XXXXXX" || true)"
    if [[ -z "${temp_link_dir}" ]]; then
      return 0
    fi
    local temp_link="${temp_link_dir}/$(basename "${symlink}")"

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

usage() {
  echo "usage: ${ME} update_dmg_mount_point installed_app_path current_version"\
  >& 2
}

main() {
  local update_dmg_mount_point="${1}"
  local installed_app_path="${2}"
  local old_version_app="${3}"

  # Early steps are critical.  Don't continue past any failure.
  set -e

  trap cleanup EXIT HUP INT QUIT TERM

  # Figure out where to install.
  if [[ ! -d "${installed_app_path}" ]]; then
    err "couldn't locate installed_app_path"
    exit 2
  fi
  note "installed_app_path = ${installed_app_path}"
  note "old_version_app = ${old_version_app}"

  # The app directory, product name, etc. can all be gotten from the
  # installed_app_path.
  readonly APP_DIR="$(basename "${installed_app_path}")"
  readonly CONTENTS_DIR="Contents"
  readonly APP_PLIST="${CONTENTS_DIR}/Info"
  readonly BUNDLE_NAME_KEY="CFBundleDisplayName"
  readonly PRODUCT_NAME=\
"$(infoplist_read "${installed_app_path}/${APP_PLIST}" "${BUNDLE_NAME_KEY}")"
  readonly FRAMEWORK_NAME="${PRODUCT_NAME} Framework"
  readonly FRAMEWORK_DIR="${FRAMEWORK_NAME}.framework"
  readonly VERSIONS_DIR_NEW=\
"${CONTENTS_DIR}/Frameworks/${FRAMEWORK_DIR}/Versions"
  readonly APP_VERSION_KEY="CFBundleShortVersionString"

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

  note "update_dmg_mount_point = ${update_dmg_mount_point}"

  if [[ -z "${update_dmg_mount_point}" ]] ||
     [[ "${update_dmg_mount_point:0:1}" != "/" ]] ||
     ! [[ -d "${update_dmg_mount_point}" ]]; then
    err "update_dmg_mount_point must be an absolute path to a directory"
    usage
    exit 3
  fi

  # The update to install.

  # update_app is the path to the new version of the .app.
  local update_app="${update_dmg_mount_point}/${APP_DIR}"
  note "update_app = ${update_app}"

  # Make sure that it's an absolute path.
  if [[ "${update_app:0:1}" != "/" ]]; then
    err "update_app must be an absolute path"
    exit 4
  fi

  if [[ ! -d "${update_app}/${VERSIONS_DIR_NEW}" ]]; then
    err "update app not using new layout"
    exit 5
  fi

  if [[ "${installed_app_path:0:1}" != "/" ]] ||
     ! [[ -d "${installed_app_path}" ]]; then
    err "installed_app_path must be an absolute path to a directory"
    exit 6
  fi

  # Figure out what the existing installed application is using for its
  # versioned directory.  This will be used later, to avoid removing the
  # existing installed version's versioned directory in case anything is still
  # using it.
  note "reading install values"

  local installed_app_path_plist="${installed_app_path}/${APP_PLIST}"
  note "installed_app_path_plist = ${installed_app_path_plist}"

  local installed_versions_dir_new="${installed_app_path}/${VERSIONS_DIR_NEW}"
  note "installed_versions_dir_new = ${installed_versions_dir_new}"

  local installed_versions_dir="${installed_versions_dir_new}"
  if [[ ! -d "${installed_versions_dir}" ]]; then
    err "installed app does not have versioned dir. Might be an old version."
    exit 7
  fi
  note "installed_versions_dir = ${installed_versions_dir}"

  # If the installed application is incredibly old, or in a skeleton bootstrap
  # installation, old_versioned_dir may not exist.
  local old_versioned_dir
  if [[ -n "${old_version_app}" ]]; then
    if [[ -d "${installed_versions_dir_new}/${old_version_app}" ]]; then
      old_versioned_dir="${installed_versions_dir_new}/${old_version_app}"
    else
      err "installed app does not have versioned dir. Might be an old version."
      exit 8
    fi
  fi
  note "old_versioned_dir = ${old_versioned_dir}"

  local update_versioned_dir=\
"${update_app}/${VERSIONS_DIR_NEW}/${UPDATE_VERSION}"
  if [[ ! -d "${update_versioned_dir}" ]]; then
    err "Update versioned dir does not have the new layout."
    exit 9
  fi

  ensure_writable_symlinks_recursive "${installed_app_path}"

  # By copying to ${installed_app_path}, the existing application name will be
  # preserved, if the user has renamed the application on disk.  Respecting
  # the user's changes is friendly.

  # Make sure that ${installed_versions_dir} exists, so that it can receive
  # the versioned directory.  It may not exist if updating from an older
  # version that did not use the same versioned layout on disk.  Later, during
  # the rsync to copy the application directory, the mode bits and timestamp on
  # ${installed_versions_dir} will be set to conform to whatever is present in
  # the update.
  #
  # ${installed_app_path} is guaranteed to exist at this point, but
  # ${installed_app_path}/${CONTENTS_DIR} may not if things are severely broken
  # or if this update is actually an initial installation from an updater
  # skeleton bootstrap.  The mkdir creates ${installed_app_path}/${CONTENTS_DIR}
  # if it doesn't exist; its mode bits will be fixed up in a subsequent rsync.
  note "creating installed_versions_dir"
  if ! mkdir -p "${installed_versions_dir}"; then
    err "mkdir of installed_versions_dir failed"
    exit 10
  fi

  local new_versioned_dir="${installed_versions_dir}/${UPDATE_VERSION}"
  note "new_versioned_dir = ${new_versioned_dir}"

  # If there's an entry at ${new_versioned_dir} but it's not a directory
  # (or it's a symbolic link, whether or not it points to a directory), rsync
  # won't get rid of it.  It's never correct to have a non-directory in place
  # of the versioned directory, so toss out whatever's there.
  if [[ -e "${new_versioned_dir}" ]] &&
     ([[ -L "${new_versioned_dir}" ]] ||
      ! [[ -d "${new_versioned_dir}" ]]); then
    note "removing non-directory in place of versioned directory"
    rm -f "${new_versioned_dir}" 2> /dev/null || true

    # If the non-directory new_versioned_dir still exists after we attempted to
    # remove it, just fail early here, before we get to the rsync.
    if [[ -e "${new_versioned_dir}" ]]; then
      err "could not remove existing file where versioned directory should be"
      exit 11
    fi
  fi

  # Copy the versioned directory.  The new versioned directory should have a
  # different name than any existing one, so this won't harm anything already
  # present in ${installed_versions_dir}, including the versioned directory
  # being used by any running processes.  If this step is interrupted, there
  # will be an incomplete versioned directory left behind, but it won't
  # interfere with anything, and it will be replaced or removed during a future
  # update attempt.
  #
  # In certain cases, same-version updates are distributed to move users
  # between channels; when this happens, the contents of the versioned
  # directories are identical and rsync will not render the versioned
  # directory unusable even for an instant.
  if [[ -n "${update_versioned_dir}" ]]; then
    note "rsyncing versioned directory"
    if ! rsync ${RSYNC_FLAGS} --delete-before "${update_versioned_dir}/" \
                                              "${new_versioned_dir}"; then
      err "rsync of versioned directory failed, status ${PIPESTATUS[0]}"

      # If the rsync of a new-layout versioned directory failed, remove it.
      # The incomplete version would break code signature validation.
      note "cleaning up new_versioned_dir"
      rm -rf "${new_versioned_dir}"
      exit 12
    fi
  fi

  # See if the timestamp of what's currently on disk is newer than the
  # update's outer .app's timestamp.  rsync will copy the update's timestamp
  # over, but if that timestamp isn't as recent as what's already on disk, the
  # .app will need to be touched.
  local needs_touch=
  if [[ "${installed_app_path}" -nt "${update_app}" ]]; then
    needs_touch="y"
  fi

  # Copy the unversioned files into place, leaving everything in
  # ${installed_versions_dir} alone.  If this step is interrupted, the
  # application will at least remain in a usable state, although it may not
  # pass signature validation.  Depending on when this step is interrupted,
  # the application will either launch the old or the new version.  The
  # critical point is when the main executable is replaced.  There isn't very
  # much to copy in this step, because most of the application is in the
  # versioned directory.  This step only accounts for around 50 files, most of
  # which are small localized InfoPlist.strings files.  Note that
  # ${VERSIONS_DIR_NEW} are included to copy their mode bits and timestamps, but
  # their contents are excluded, having already been installed above. The
  # ${VERSIONS_DIR_NEW}/Current symbolic link is updated or created in this
  # step, however.
  note "rsyncing app directory"
  if ! rsync ${RSYNC_FLAGS} --delete-after \
       --include="/${VERSIONS_DIR_NEW}/Current" \
       --exclude="/${VERSIONS_DIR_NEW}/*" "${update_app}/" \
       "${installed_app_path}"; then
    err "rsync of app directory failed, status ${PIPESTATUS[0]}"
    exit 13
  fi

  note "rsyncs complete"

  if [[ -n "${g_temp_dir}" ]]; then
    # The temporary directory, if any, is no longer needed.
    rm -rf "${g_temp_dir}" 2> /dev/null || true
    g_temp_dir=
    note "g_temp_dir = ${g_temp_dir}"
  fi

  # If necessary, touch the outermost .app so that it appears to the outside
  # world that something was done to the bundle.  This will cause
  # LaunchServices to invalidate the information it has cached about the
  # bundle even if lsregister does not run.  This is not done if rsync already
  # updated the timestamp to something newer than what had been on disk.  This
  # is not considered a critical step, and if it fails, this script will not
  # exit.
  if [[ -n "${needs_touch}" ]]; then
    touch -cf "${installed_app_path}" || true
  fi

  # Read the new values, such as the version.
  note "reading new values"

  local new_version_app
  if ! new_version_app="$(infoplist_read "${installed_app_path_plist}" \
                                         "${APP_VERSION_KEY}")" ||
     [[ -z "${new_version_app}" ]]; then
    err "couldn't determine new_version_app"
    exit 14
  fi

  local new_versioned_dir="${installed_versions_dir}/${new_version_app}"

  # Make sure that the update was successful by comparing the version found in
  # the update with the version now on disk.
  if [[ "${new_version_app}" != "${UPDATE_VERSION}" ]]; then
    err "new_version_app and UPDATE_VERSION do not match"
    exit 15
  fi

  # Notify LaunchServices.  This is not considered a critical step, and
  # lsregister's exit codes shouldn't be confused with this script's own.
  # Redirect stdout to /dev/null to suppress the useless "ThrottleProcessIO:
  # throttling disk i/o" messages that lsregister might print.
  note "notifying LaunchServices"
  local coreservices="/System/Library/Frameworks/CoreServices.framework"
  local launchservices="${coreservices}/Frameworks/LaunchServices.framework"
  local lsregister="${launchservices}/Support/lsregister"
  "${lsregister}" -f "${installed_app_path}" > /dev/null || true

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
  for versioned_dir in "${installed_versions_dir_new}/"*; do
    note "versioned_dir = ${versioned_dir}"
    if [[ "${versioned_dir}" = "${new_versioned_dir}" ]] ||
       [[ "${versioned_dir}" = "${old_versioned_dir}" ]] ||
       [[ "${versioned_dir}" = "${installed_versions_dir_new}/Current" ]]; then
      # This is the versioned directory corresponding to the update that was
      # just applied or the version that was previously in use.  Leave it
      # alone.
      continue
    fi

    # Look for any processes whose executables are within this versioned
    # directory.  They'll be helper processes, such as renderers.  Their
    # existence indicates that this versioned directory is currently in use.
    local ps_string="${versioned_dir}/"

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

  note "setting permissions"

  local chmod_mode="a+rX,u+w,go-w"
  if [[ "${installed_app_path:0:14}" = "/Applications/" ]] &&
    chgrp -Rh admin "${installed_app_path}" 2> /dev/null; then
    chmod_mode="a+rX,ug+w,o-w"
  else
    chown -Rh root:wheel "${installed_app_path}" 2> /dev/null
  fi

  note "chmod_mode = ${chmod_mode}"
  chmod -R "${chmod_mode}" "${installed_app_path}" 2> /dev/null

  # On the Mac, or at least on HFS+, symbolic link permissions are significant,
  # but chmod -R and -h can't be used together.  Do another pass to fix the
  # permissions on any symbolic links.
  find "${installed_app_path}" -type l -exec chmod -h "${chmod_mode}" {} + \
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

  xattr -d -r "${QUARANTINE_ATTR}" "${installed_app_path}" 2> /dev/null

  # Great success!
  note "done!"

  trap - EXIT

  return 0
}

# Check "less than" instead of "not equal to" in case there are changes to pass
# more arguments.
if [[ ${#} -lt 3 ]]; then
  usage
  echo ${#} >& 1
  exit 16
fi

main "${@}"
exit ${?}
