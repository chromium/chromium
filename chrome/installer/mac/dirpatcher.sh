#!/bin/bash -p

# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# usage: dirpatcher.sh old_dir patch_dir new_dir
#
# dirpatcher creates new_dir from patch_dir by decompressing and copying
# files, and using goobspatch to apply binary diffs to files in old_dir.
#
# dirpatcher performs the inverse operation to dirdiffer. For more details,
# consult dirdiffer.sh.
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
# 11  Unrecognized file extension
# 12  Attempt to patch a nonexistent or non-regular file
# 13  Patch application failed
# 14  File decompression failed
# 15  File copy failed
# 16  Could not set mode (permissions)
# 17  Could not set modification time

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
GOOBSPATCH="$(find_tool goobspatch)"
readonly GOOBSPATCH
readonly BUNZIP2="bunzip2"
readonly GUNZIP="gunzip"
XZDEC="$(find_tool xzdec)"
readonly XZDEC
readonly GBS_SUFFIX='$gbs'
readonly BZ2_SUFFIX='$bz2'
readonly GZ_SUFFIX='$gz'
readonly XZ_SUFFIX='$xz'
readonly PLAIN_SUFFIX='$raw'

err() {
  local error="${1}"

  echo "${ME}: ${error}" >& 2
}

declare -a g_cleanup
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
  local patch_file="${1}"
  local new_file="${2}"

  local mode
  mode="$(stat "-f%OMp%OLp" "${patch_file}")"
  if ! chmod -h "${mode}" "${new_file}"; then
    exit 16
  fi

  if ! [[ -L "${new_file}" ]]; then
    # Symbolic link modification times can't be copied because there's no
    # shell tool that provides direct access to lutimes. Instead, the symbolic
    # link was created with rsync, which already copied the timestamp with
    # lutimes.
    if ! touch -r "${patch_file}" "${new_file}"; then
      exit 17
    fi
  fi
}

apply_patch() {
  local old_file="${1}"
  local patch_file="${2}"
  local new_file="${3}"
  local patcher="${4}"

  if [[ -L "${old_file}" ]] || ! [[ -f "${old_file}" ]]; then
    err "can't patch nonexistent or irregular file ${old_file}"
    exit 12
  fi

  if ! "${patcher}" "${old_file}" "${new_file}" "${patch_file}"; then
    err "couldn't create ${new_file} by applying ${patch_file} to ${old_file}"
    exit 13
  fi
}

decompress_file() {
  local old_file="${1}"
  local patch_file="${2}"
  local new_file="${3}"
  local decompressor="${4}"

  if ! "${decompressor}" -c < "${patch_file}" > "${new_file}"; then
    err "couldn't decompress ${patch_file} to ${new_file} with ${decompressor}"
    exit 14
  fi
}

copy_file() {
  local old_file="${1}"
  local patch_file="${2}"
  local new_file="${3}"
  local extra="${4}"

  if ! cp "${patch_file}" "${new_file}"; then
    exit 15
  fi
}

patch_file() {
  local old_file="${1}"
  local patch_file="${2}"
  local new_file="${3}"

  local operation extra strip_length

  if [[ "${patch_file: -${#GBS_SUFFIX}}" = "${GBS_SUFFIX}" ]]; then
    operation="apply_patch"
    extra="${GOOBSPATCH}"
    strip_length=${#GBS_SUFFIX}
  elif [[ "${patch_file: -${#BZ2_SUFFIX}}" = "${BZ2_SUFFIX}" ]]; then
    operation="decompress_file"
    extra="${BUNZIP2}"
    strip_length=${#BZ2_SUFFIX}
  elif [[ "${patch_file: -${#GZ_SUFFIX}}" = "${GZ_SUFFIX}" ]]; then
    operation="decompress_file"
    extra="${GUNZIP}"
    strip_length=${#GZ_SUFFIX}
  elif [[ "${patch_file: -${#XZ_SUFFIX}}" = "${XZ_SUFFIX}" ]]; then
    operation="decompress_file"
    extra="${XZDEC}"
    strip_length=${#XZ_SUFFIX}
  elif [[ "${patch_file: -${#PLAIN_SUFFIX}}" = "${PLAIN_SUFFIX}" ]]; then
    operation="copy_file"
    extra="patch"
    strip_length=${#PLAIN_SUFFIX}
  else
    err "don't know how to operate on ${patch_file}"
    exit 11
  fi

  old_file="${old_file:0:${#old_file} - ${strip_length}}"
  new_file="${new_file:0:${#new_file} - ${strip_length}}"

  if [[ -e "${new_file}" ]]; then
    err "${new_file} already exists"
    exit 8
  fi

  "${operation}" "${old_file}" "${patch_file}" "${new_file}" "${extra}"

  copy_mode_and_time "${patch_file}" "${new_file}"
}

patch_symlink() {
  local patch_file="${1}"
  local new_file="${2}"

  # local target
  # target="$(readlink "${patch_file}")"
  # ln -s "${target}" "${new_file}"

  # Use rsync instead of the above, as it's the only way to preserve the
  # timestamp of a symbolic link using shell tools.
  if ! rsync --links --times "${patch_file}" "${new_file}"; then
    exit 10
  fi

  copy_mode_and_time "${patch_file}" "${new_file}"
}

patch_dir() {
  local old_dir="${1}"
  local patch_dir="${2}"
  local new_dir="${3}"

  if ! mkdir "${new_dir}"; then
    exit 7
  fi

  local patch_file
  for patch_file in "${patch_dir}/"*; do
    local file="${patch_file:${#patch_dir} + 1}"
    local old_file="${old_dir}/${file}"
    local new_file="${new_dir}/${file}"

    if [[ -e "${new_file}" ]]; then
      err "${new_file} already exists"
      exit 8
    fi

    if [[ -L "${patch_file}" ]]; then
      patch_symlink "${patch_file}" "${new_file}"
    elif [[ -d "${patch_file}" ]]; then
      patch_dir "${old_file}" "${patch_file}" "${new_file}"
    elif ! [[ -f "${patch_file}" ]]; then
      err "can't handle irregular file ${patch_file}"
      exit 9
    else
      patch_file "${old_file}" "${patch_file}" "${new_file}"
    fi
  done

  copy_mode_and_time "${patch_dir}" "${new_dir}"
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
  echo "usage: ${ME} old_dir patch_dir new_dir" >& 2
}

main() {
  local old_dir patch_dir new_dir
  old_dir="$(shell_safe_path "${1}")"
  patch_dir="$(shell_safe_path "${2}")"
  new_dir="$(shell_safe_path "${3}")"

  trap cleanup EXIT HUP INT QUIT TERM

  if ! [[ -d "${old_dir}" ]] || ! [[ -d "${patch_dir}" ]]; then
    err "old_dir and patch_dir must exist and be directories"
    usage
    exit 3
  fi

  if [[ -e "${new_dir}" ]]; then
    err "new_dir must not exist"
    usage
    exit 4
  fi

  local new_dir_parent
  new_dir_parent="$(dirname "${new_dir}")"
  if ! [[ -d "${new_dir_parent}" ]]; then
    err "new_dir parent directory must exist and be a directory"
    usage
    exit 5
  fi

  local old_dir_phys patch_dir_phys new_dir_parent_phys new_dir_phys
  old_dir_phys="$(cd "${old_dir}" && pwd -P)"
  patch_dir_phys="$(cd "${patch_dir}" && pwd -P)"
  new_dir_parent_phys="$(cd "${new_dir_parent}" && pwd -P)"
  new_dir_phys="${new_dir_parent_phys}/$(basename "${new_dir}")"

  if dirs_contained "${old_dir_phys}" "${patch_dir_phys}" ||
     dirs_contained "${old_dir_phys}" "${new_dir_phys}" ||
     dirs_contained "${patch_dir_phys}" "${new_dir_phys}"; then
    err "directories must not contain one another"
    usage
    exit 6
  fi

  g_cleanup+=("${new_dir}")

  patch_dir "${old_dir}" "${patch_dir}" "${new_dir}"

  unset g_cleanup[${#g_cleanup[@]}]
  trap - EXIT
}

if [[ ${#} -ne 3 ]]; then
  usage
  exit 2
fi

main "${@}"
exit ${?}
