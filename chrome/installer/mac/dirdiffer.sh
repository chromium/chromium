#!/bin/bash -p

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# usage: dirdiffer.sh old_dir new_dir patch_dir
#
# dirdiffer creates a patch directory patch_dir that represents the difference
# between old_dir and new_dir. patch_dir can be used with dirpatcher to
# recreate new_dir given old_dir.
#
# dirdiffer operates recursively, properly handling ordinary files, symbolic
# links, and directories, as they are found in new_dir. Symbolic links and
# directories are always replicated as-is in patch_dir. Ordinary files will
# be represented at the appropriate location in patch_dir by one of the
# following:
#
#  - a binary diff prepared by goobsdiff that can transform the file at the
#    same position in old_dir to the version in new_dir, but only when such a
#    file already exists in old_dir and is an ordinary file. These files are
#    given a "$gbs" suffix.
#  - a bzip2-compressed copy of the new file from new_dir; in patch_dir, the
#    new file will have a "$bz2" suffix.
#  - a gzip-compressed copy of the new file from new_dir; in patch_dir, the
#    new file will have a "$gz" suffix.
#  - an xz/lzma2-compressed copy of the new file from new_dir; in patch_dir,
#    the new file will have an "$xz" suffix.
#  - an uncompressed copy of the new file from new_dir; in patch_dir, the
#    new file will have a "$raw" suffix.
#
# The unconventional suffixes are used because they aren't likely to occur in
# filenames.
#
# Of these options, the smallest possible representation is chosen. Note that
# goobsdiff itself will also compress various sections of a binary diff with
# bzip2, gzip, or xz/lzma2, or leave them uncompressed, according to which is
# smallest. The approach of choosing the smallest possible representation is
# time-consuming but given the choices of compressors results in an overall
# size reduction of about 3%-5% relative to using bzip2 as the only
# compressor; bzip2 is generally more effective for these data sets than gzip,
# and xz/lzma2 more effective than bzip2.
#
# For large input files, goobsdiff is also very time-consuming and
# memory-intensive. The overall "wall clock time" spent preparing a patch_dir
# representing the differences between Google Chrome's 6.0.422.0 and 6.0.427.0
# versioned directories from successive weekly dev channel releases on a
# 2.53GHz dual-core 4GB MacBook Pro is 3 minutes. Reconstructing new_dir with
# dirpatcher is much quicker; in the above configuration, only 10 seconds are
# needed for reconstruction.
#
# After creating a full patch_dir structure, but before returning, dirpatcher
# is invoked to attempt to recreate new_dir in a temporary location given
# old_dir and patch_dir. The recreated new_dir is then compared against the
# original new_dir as a verification step. Should verification fail, dirdiffer
# exits with a nonzero status, and patch_dir should not be used.
#
# Environment variables:
# DIRDIFFER_EXCLUDE
#   When an entry in new_dir matches this regular expression, it will not be
#   included in patch_dir. All prospective paths in new_dir will be matched
#   against this regular expression, including directories. If a directory
#   matches this pattern, dirdiffer will also ignore the directory's contents.
# DIRDIFFER_NO_DIFF
#   When an entry in new_dir matches this regular expression, it will not be
#   represented in patch_dir by a $gbs file prepared by goobsdiff. It will only
#   appear as a $bz2, $gz, or $raw file. Only files in new_dir, not
#   directories,  will be matched against this regular expression.
#
# Exit codes:
#  0  OK
#  1  Unknown failure
#  2  Incorrect number of parameters
#  3  Input directories do not exist or are not directories
#  4  Output directory already exists
#  5  Parent of output directory does not exist or is not a directory
#  6  An input or output directories contains another
#  7  Could not create output directory
#  8  File already exists in output directory
#  9  Found an irregular file (non-directory, file, or symbolic link) in input
# 10  Could not create symbolic link
# 11  File copy failed
# 12  bzip2 compression failed
# 13  gzip compression failed
# 14  xz/lzma2 compression failed
# 15  Patch creation failed
# 16  Verification failed
# 17  Could not set mode (permissions)
# 18  Could not set modification time
# 19  Invalid regular expression (irregular expression?)

set -eu

# Environment sanitization. Set a known-safe PATH. Clear environment variables
# that might impact the interpreter's operation. The |bash -p| invocation
# on the #! line takes the bite out of BASH_ENV, ENV, and SHELLOPTS (among
# other features), but clearing them here ensures that they won't impact any
# shell scripts used as utility programs. SHELLOPTS is read-only and can't be
# unset, only unexported.
export PATH="/usr/bin:/bin:/usr/sbin:/sbin"
unset BASH_ENV CDPATH ENV GLOBIGNORE IFS POSIXLY_CORRECT
export -n SHELLOPTS

shopt -s dotglob nullglob

# find_tool looks for an executable file named |tool_name|:
#  - in the same directory as this script,
#  - if this script is located in a Chromium source tree, at the expected
#    Release output location in the Mac out directory,
#  - as above, but in the Debug output location
# If found in any of the above locations, the script's path is output.
# Otherwise, this function outputs |tool_name| as a fallback, allowing it to
# be found (or not) by an ordinary ${PATH} search.
find_tool() {
  local tool_name="${1}"

  local script_dir
  script_dir="$(dirname "${0}")"

  local tool="${script_dir}/${tool_name}"
  if [[ -f "${tool}" ]] && [[ -x "${tool}" ]]; then
    echo "${tool}"
    return
  fi

  local script_dir_phys
  script_dir_phys="$(cd "${script_dir}" && pwd -P)"
  if [[ "${script_dir_phys}" =~ ^(.*)/src/chrome/installer/mac$ ]]; then
    tool="${BASH_REMATCH[1]}/src/out/Release/${tool_name}"
    if [[ -f "${tool}" ]] && [[ -x "${tool}" ]]; then
      echo "${tool}"
      return
    fi

    tool="${BASH_REMATCH[1]}/src/out/Debug/${tool_name}"
    if [[ -f "${tool}" ]] && [[ -x "${tool}" ]]; then
      echo "${tool}"
      return
    fi
  fi

  echo "${tool_name}"
}

ME="$(basename "${0}")"
readonly ME
DIRPATCHER="$(dirname "${0}")/dirpatcher.sh"
readonly DIRPATCHER
GOOBSDIFF="$(find_tool goobsdiff)"
readonly GOOBSDIFF
readonly BZIP2="bzip2"
readonly GZIP="gzip"
XZ="$(find_tool xz)"
readonly XZ
readonly GBS_SUFFIX='$gbs'
readonly BZ2_SUFFIX='$bz2'
readonly GZ_SUFFIX='$gz'
readonly XZ_SUFFIX='$xz'
readonly PLAIN_SUFFIX='$raw'

# Workaround for http://code.google.com/p/chromium/issues/detail?id=83180#c3
# In bash 4.0, "declare VAR" no longer initializes VAR if not already set.
: ${DIRDIFFER_EXCLUDE:=}
: ${DIRDIFFER_NO_DIFF:=}

err() {
  local error="${1}"

  echo "${ME}: ${error}" >& 2
}

declare -a g_cleanup g_verify_exclude
cleanup() {
  local status=${?}

  trap - EXIT
  trap '' HUP INT QUIT TERM

  if [[ ${status} -ge 128 ]]; then
    err "Caught signal $((${status} - 128))"
  fi

  if [[ "${#g_cleanup[@]}" -gt 0 ]]; then
    rm -rf "${g_cleanup[@]}"
  fi

  exit ${status}
}

copy_mode_and_time() {
  local new_file="${1}"
  local patch_file="${2}"

  local mode
  mode="$(stat "-f%OMp%OLp" "${new_file}")"
  if ! chmod -h "${mode}" "${patch_file}"; then
    exit 17
  fi

  if ! [[ -L "${patch_file}" ]]; then
    # Symbolic link modification times can't be copied because there's no
    # shell tool that provides direct access to lutimes. Instead, the symbolic
    # link was created with rsync, which already copied the timestamp with
    # lutimes.
    if ! touch -r "${new_file}" "${patch_file}"; then
      exit 18
    fi
  fi
}

file_size() {
  local file="${1}"

  stat -f %z "${file}"
}

make_patch_file() {
  local old_file="${1}"
  local new_file="${2}"
  local patch_file="${3}"

  local uncompressed_file="${patch_file}${PLAIN_SUFFIX}"
  if ! cp "${new_file}" "${uncompressed_file}"; then
    exit 11
  fi
  local uncompressed_size
  uncompressed_size="$(file_size "${new_file}")"

  local keep_file="${uncompressed_file}"
  local keep_size="${uncompressed_size}"

  local bz2_file="${patch_file}${BZ2_SUFFIX}"
  if [[ -e "${bz2_file}" ]]; then
    err "${bz2_file} already exists"
    exit 8
  fi
  if ! "${BZIP2}" -9c < "${new_file}" > "${bz2_file}"; then
    err "couldn't compress ${new_file} to ${bz2_file} with ${BZIP2}"
    exit 12
  fi
  local bz2_size
  bz2_size="$(file_size "${bz2_file}")"

  if [[ "${bz2_size}" -ge "${keep_size}" ]]; then
    rm -f "${bz2_file}"
  else
    rm -f "${keep_file}"
    keep_file="${bz2_file}"
    keep_size="${bz2_size}"
  fi

  local gz_file="${patch_file}${GZ_SUFFIX}"
  if [[ -e "${gz_file}" ]]; then
    err "${gz_file} already exists"
    exit 8
  fi
  if ! "${GZIP}" -9cn < "${new_file}" > "${gz_file}"; then
    err "couldn't compress ${new_file} to ${gz_file} with ${GZIP}"
    exit 13
  fi
  local gz_size
  gz_size="$(file_size "${gz_file}")"

  if [[ "${gz_size}" -ge "${keep_size}" ]]; then
    rm -f "${gz_file}"
  else
    rm -f "${keep_file}"
    keep_file="${gz_file}"
    keep_size="${gz_size}"
  fi

  local xz_flags=("-c")

  # If the file looks like a Mach-O file, including a universal/fat file, add
  # the x86 BCJ filter, which results in slightly better compression of x86
  # and x86_64 executables. Mach-O files might contain other architectures,
  # but they aren't currently expected in Chrome.
  local file_output
  file_output="$(file "${new_file}" 2> /dev/null || true)"
  if [[ "${file_output}" =~ Mach-O ]]; then
    xz_flags+=("--x86")
  fi

  # Use an lzma2 encoder. This is equivalent to xz -9 -e, but allows filters
  # to precede the compressor.
  xz_flags+=("--lzma2=preset=9e")

  local xz_file="${patch_file}${XZ_SUFFIX}"
  if [[ -e "${xz_file}" ]]; then
    err "${xz_file} already exists"
    exit 8
  fi
  if ! "${XZ}" "${xz_flags[@]}" < "${new_file}" > "${xz_file}"; then
    err "couldn't compress ${new_file} to ${xz_file} with ${XZ}"
    exit 14
  fi
  local xz_size
  xz_size="$(file_size "${xz_file}")"

  if [[ "${xz_size}" -ge "${keep_size}" ]]; then
    rm -f "${xz_file}"
  else
    rm -f "${keep_file}"
    keep_file="${xz_file}"
    keep_size="${xz_size}"
  fi

  if [[ -f "${old_file}" ]] && ! [[ -L "${old_file}" ]] &&
     ! [[ "${new_file}" =~ ${DIRDIFFER_NO_DIFF} ]]; then
    local gbs_file="${patch_file}${GBS_SUFFIX}"
    if [[ -e "${gbs_file}" ]]; then
      err "${gbs_file} already exists"
      exit 8
    fi
    if ! "${GOOBSDIFF}" "${old_file}" "${new_file}" "${gbs_file}"; then
      err "couldn't create ${gbs_file} by comparing ${old_file} to ${new_file}"
      exit 15
    fi
    local gbs_size
    gbs_size="$(file_size "${gbs_file}")"

    if [[ "${gbs_size}" -ge "${keep_size}" ]]; then
      rm -f "${gbs_file}"
    else
      rm -f "${keep_file}"
      keep_file="${gbs_file}"
      keep_size="${gbs_size}"
    fi
  fi

  copy_mode_and_time "${new_file}" "${keep_file}"
}

make_patch_symlink() {
  local new_file="${1}"
  local patch_file="${2}"

  # local target
  # target="$(readlink "${new_file}")"
  # ln -s "${target}" "${patch_file}"

  # Use rsync instead of the above, as it's the only way to preserve the
  # timestamp of a symbolic link using shell tools.
  if ! rsync --links --times "${new_file}" "${patch_file}"; then
    exit 10
  fi

  copy_mode_and_time "${new_file}" "${patch_file}"
}

make_patch_dir() {
  local old_dir="${1}"
  local new_dir="${2}"
  local patch_dir="${3}"

  if ! mkdir "${patch_dir}"; then
    exit 7
  fi

  local new_file
  for new_file in "${new_dir}/"*; do
    local file="${new_file:${#new_dir} + 1}"
    local old_file="${old_dir}/${file}"
    local patch_file="${patch_dir}/${file}"

    if [[ "${new_file}" =~ ${DIRDIFFER_EXCLUDE} ]]; then
      g_verify_exclude+=("${new_file}")
      continue
    fi

    if [[ -e "${patch_file}" ]]; then
      err "${patch_file} already exists"
      exit 8
    fi

    if [[ -L "${new_file}" ]]; then
      make_patch_symlink "${new_file}" "${patch_file}"
    elif [[ -d "${new_file}" ]]; then
      make_patch_dir "${old_file}" "${new_file}" "${patch_file}"
    elif [[ ! -f "${new_file}" ]]; then
      err "can't handle irregular file ${new_file}"
      exit 9
    else
      make_patch_file "${old_file}" "${new_file}" "${patch_file}"
    fi
  done

  copy_mode_and_time "${new_dir}" "${patch_dir}"
}

verify_patch_dir() {
  local old_dir="${1}"
  local new_dir="${2}"
  local patch_dir="${3}"

  local verify_temp_dir verify_dir
  verify_temp_dir="$(mktemp -d -t "${ME}")"
  g_cleanup+=("${verify_temp_dir}")
  verify_dir="${verify_temp_dir}/patched"

  if ! "${DIRPATCHER}" "${old_dir}" "${patch_dir}" "${verify_dir}"; then
    err "patch application for verification failed"
    exit 16
  fi

  # rsync will print a line for any file, directory, or symbolic link that
  # differs or exists only in one directory. As used here, it correctly
  # considers link targets, file contents, permissions, and timestamps.
  local rsync_command=(rsync --checksum --delete --links --out-format=%n \
                       --perms --recursive --times \
                       "${new_dir}/" "${verify_dir}")
  if [[ ${#g_verify_exclude[@]} -gt 0 ]]; then
    local exclude
    for exclude in "${g_verify_exclude[@]}"; do
      # ${g_verify_exclude[@]} contains paths in ${new_dir}. Strip off
      # ${new_dir} from the beginning of each, but leave a leading "/" so that
      # rsync treats them as being at the root of the "transfer."
      rsync_command+=("--exclude=${exclude:${#new_dir}}")
    done
  fi

  local rsync_output
  if ! rsync_output="$("${rsync_command[@]}")"; then
    err "rsync for verification failed"
    exit 16
  fi

  rm -rf "${verify_temp_dir}"
  unset g_cleanup[${#g_cleanup[@]}]

  if [[ -n "${rsync_output}" ]]; then
    err "verification failed"
    exit 16
  fi
}

# shell_safe_path ensures that |path| is safe to pass to tools as a
# command-line argument. If the first character in |path| is "-", "./" is
# prepended to it. The possibly-modified |path| is output.
shell_safe_path() {
  local path="${1}"
  if [[ "${path:0:1}" = "-" ]]; then
    echo "./${path}"
  else
    echo "${path}"
  fi
}

dirs_contained() {
  local dir1="${1}/"
  local dir2="${2}/"

  if [[ "${dir1:0:${#dir2}}" = "${dir2}" ]] ||
     [[ "${dir2:0:${#dir1}}" = "${dir1}" ]]; then
    return 0
  fi

  return 1
}

usage() {
  echo "usage: ${ME} old_dir new_dir patch_dir" >& 2
}

main() {
  local old_dir new_dir patch_dir
  old_dir="$(shell_safe_path "${1}")"
  new_dir="$(shell_safe_path "${2}")"
  patch_dir="$(shell_safe_path "${3}")"

  trap cleanup EXIT HUP INT QUIT TERM

  if ! [[ -d "${old_dir}" ]] || ! [[ -d "${new_dir}" ]]; then
    err "old_dir and new_dir must exist and be directories"
    usage
    exit 3
  fi

  if [[ -e "${patch_dir}" ]]; then
    err "patch_dir must not exist"
    usage
    exit 4
  fi

  local patch_dir_parent
  patch_dir_parent="$(dirname "${patch_dir}")"
  if ! [[ -d "${patch_dir_parent}" ]]; then
    err "patch_dir parent directory must exist and be a directory"
    usage
    exit 5
  fi

  # The weird conditional structure is because the status of the RE comparison
  # needs to be available in ${?} without conflating it with other conditions
  # or negating it. Only a status of 2 from the =~ operator indicates an
  # invalid regular expression.

  if [[ -n "${DIRDIFFER_EXCLUDE}" ]]; then
    if [[ "" =~ ${DIRDIFFER_EXCLUDE} ]]; then
      true
    elif [[ ${?} -eq 2 ]]; then
      err "DIRDIFFER_EXCLUDE contains an invalid regular expression"
      exit 19
    fi
  fi

  if [[ -n "${DIRDIFFER_NO_DIFF}" ]]; then
    if [[ "" =~ ${DIRDIFFER_NO_DIFF} ]]; then
      true
    elif [[ ${?} -eq 2 ]]; then
      err "DIRDIFFER_NO_DIFF contains an invalid regular expression"
      exit 19
    fi
  fi

  local old_dir_phys new_dir_phys patch_dir_parent_phys patch_dir_phys
  old_dir_phys="$(cd "${old_dir}" && pwd -P)"
  new_dir_phys="$(cd "${new_dir}" && pwd -P)"
  patch_dir_parent_phys="$(cd "${patch_dir_parent}" && pwd -P)"
  patch_dir_phys="${patch_dir_parent_phys}/$(basename "${patch_dir}")"

  if dirs_contained "${old_dir_phys}" "${new_dir_phys}" ||
     dirs_contained "${old_dir_phys}" "${patch_dir_phys}" ||
     dirs_contained "${new_dir_phys}" "${patch_dir_phys}"; then
    err "directories must not contain one another"
    usage
    exit 6
  fi

  g_cleanup[${#g_cleanup[@]}]="${patch_dir}"

  make_patch_dir "${old_dir}" "${new_dir}" "${patch_dir}"

  verify_patch_dir "${old_dir}" "${new_dir}" "${patch_dir}"

  unset g_cleanup[${#g_cleanup[@]}]
  trap - EXIT
}

if [[ ${#} -ne 3 ]]; then
  usage
  exit 2
fi

main "${@}"
exit ${?}
