#!/bin/bash

# Copyright 2023 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Script to generate `gn desc` json outputs that are used as an input to the
# gn2bp conversion tool.
# Inputs:
#  Arguments:
#   -d dir: The directory that points to a local checkout of chromium/src.
#   -r rev: The reference revision of upstream Chromium to use. Must match the
#           last revision that has been imported using import_cronet.sh.
#  Optional arguments:
#   -f: Force reset the chromium/src directory.

set -e -x

OPTSTRING=d:fr:

usage() {
    cat <<EOF
Usage: gen_gn_desc.sh -d dir -r rev [-f]
EOF
    exit 1
}


# Run this script inside a full chromium checkout.

OUT_PATH="out/cronet"

#######################################
# Sets the chromium/src repository to a reference state.
# Arguments:
#   rev, string
#   chromium_dir, string
#   force_reset, boolean
#######################################
function setup_chromium_src_repo() (
  local -r rev="$1"
  local -r chromium_dir="$2"
  local -r force_reset="$3"

  cd "${chromium_dir}"
  git fetch --tags

  if [ -n "${force_reset}" ]; then
    git reset --hard
  fi

  git checkout "${rev}"
  gclient sync \
    --no-history \
    --shallow \
    --delete_unversioned_trees
)

#######################################
# Imports intermediate CLs for correct generation of desc_*.json
# Arguments:
#   chromium_dir, string
#######################################
function cherry_pick_chromium_cls() (
  cd "${chromium_dir}"
  # Delete once 130.0.6675.0 is imported.
  git fetch https://chromium.googlesource.com/chromium/src refs/changes/44/5808944/6 && git cherry-pick FETCH_HEAD
  # Delete once 130.0.6675.0 is imported.
  git fetch https://chromium.googlesource.com/chromium/src refs/changes/45/5808945/6 && git cherry-pick FETCH_HEAD
  # Delete once 130.0.6675.0 is imported.
  git fetch https://chromium.googlesource.com/chromium/src refs/changes/78/5809278/6 && git cherry-pick FETCH_HEAD
  # Delete once 130.0.6675.0 is imported.
  git fetch https://chromium.googlesource.com/chromium/src refs/changes/40/5806340/8 && git cherry-pick FETCH_HEAD
  # Delete once 130.0.6682.0 is imported.
  git fetch https://chromium.googlesource.com/chromium/src refs/changes/80/5809380/6 && git cherry-pick FETCH_HEAD
)
#######################################
# Generate desc.json for a specified architecture.
# Globals:
#   ANDROID_BUILD_TOP
#   OUT_PATH
# Arguments:
#   target_cpu, string
#   chromium_dir, string
#######################################
function gn_desc() (
  local -r target_cpu="$1"
  local -r chromium_dir="$2"
  local -a gn_args=(
    "target_os = \"android\""
    "enable_websockets = false"
    "disable_file_support = true"
    "is_component_build = false"
    "use_partition_alloc = false"
    "include_transport_security_state_preload_list = false"
    "use_platform_icu_alternatives = true"
    "default_min_sdk_version = 23"
    "enable_reporting = true"
    "use_hashed_jni_names = true"
    "enable_base_tracing = false"
    "is_cronet_build = true"
    "is_debug = false"
    "is_official_build = true"
    "use_nss_certs = false"
    "clang_use_default_sample_profile = false"
    "media_use_ffmpeg=false"
    "use_thin_lto=false"
    "enable_resource_allowlist_generation=false"
    "exclude_unwind_tables=true"
    "symbol_level=1"
    "enable_rust=false"
    "is_cronet_for_aosp_build=true"
  )
  gn_args+=("target_cpu = \"${target_cpu}\"")

  # Only set arm_use_neon on arm architectures to prevent warning from being
  # written to json output.
  if [[ "${target_cpu}" = "arm" ]]; then
    gn_args+=("arm_use_neon = false")
  fi

  cd "${chromium_dir}"

  # Configure gn args.
  gn gen "${OUT_PATH}" --args="${gn_args[*]}"

  # Generate desc.json.
  local -r out_file="${ANDROID_BUILD_TOP}/external/cronet/android/tools/gn2bp/desc_${target_cpu}.json"
  gn desc "${OUT_PATH}" --format=json --all-toolchains "//*" > "${out_file}"
)

while getopts "${OPTSTRING}" opt; do
  case "${opt}" in
    d) chromium_dir="${OPTARG}" ;;
    f) force_reset=true ;;
    r) rev="${OPTARG}" ;;
    ?) usage ;;
    *) echo "'${opt}' '${OPTARG}'"
  esac
done

if [ -z "${chromium_dir}" ]; then
  echo "-d argument required"
  usage
fi

if [ -z "${rev}" ]; then
  echo "-r argument required"
  usage
fi

if [ -z "${ANDROID_BUILD_TOP}" ]; then
    echo "ANDROID_BUILD_TOP is not set. Please run source build/envsetup.sh && lunch"
    exit 1
fi


setup_chromium_src_repo "${rev}" "${chromium_dir}" "${force_reset}"
cherry_pick_chromium_cls "${chromium_dir}"
gn_desc x86 "${chromium_dir}"
gn_desc x64 "${chromium_dir}"
gn_desc arm "${chromium_dir}"
gn_desc arm64 "${chromium_dir}"
gn_desc riscv64 "${chromium_dir}"
