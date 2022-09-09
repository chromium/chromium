#!/bin/sh

# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -eu

THIS_DIR=$(dirname "$0")

OUT_DIR="$1"

if [[ ! "$1" ]]; then
  echo "Usage: $(basename "$0") [output_dir]"
  exit 1
fi

if [[ -e "$1" && ! -d "$1" ]]; then
  echo "Output directory \`$1' exists but is not a directory."
  exit 1
fi
if [[ ! -d "$1" ]]; then
  mkdir -p "$1"
fi

generate_test_data() {
  # `hdiutil convert` cannot overwrite files, so remove items in the output
  # directory.
  rm -f "${OUT_DIR}"/*

  # HFS Raw Images #############################################################

  MAKE_HFS="${THIS_DIR}/make_hfs.sh"
  "${MAKE_HFS}" HFS+ 1024 "${OUT_DIR}/hfs_plus.img"
  "${MAKE_HFS}" hfsx $((8 * 1024)) "${OUT_DIR}/hfsx_case_sensitive.img"

  # DMG Files ##################################################################

  DMG_SOURCE=$(mktemp -d -t dmg_generate_test_data.XXXXXX)
  echo "This is a test DMG file. It has been generated from " \
      "chrome/test/data/safe_browsing/dmg/generate_test_data.sh" \
          > "${DMG_SOURCE}/README.txt"
  dd if=/dev/urandom of="${DMG_SOURCE}/random" bs=512 count=4 &> /dev/null

  DMG_TEMPLATE_FORMAT="UDRO"
  DMG_FORMATS="UDRW UDCO UDZO UDBZ UFBI UDTO UDSP"
  DMG_LAYOUTS="SPUD GPTSPUD NONE"

  # First create uncompressed template DMGs in the three partition layouts.
  for layout in ${DMG_LAYOUTS}; do
    DMG_NAME="dmg_${DMG_TEMPLATE_FORMAT}_${layout}"
    hdiutil create -srcfolder "${DMG_SOURCE}" \
      -format "${DMG_TEMPLATE_FORMAT}" -layout "${layout}" \
      -fs JHFS+ -volname "${DMG_NAME}" \
      "${OUT_DIR}/${DMG_NAME}"
  done

  # Convert each template into the different compression format.
  for format in ${DMG_FORMATS}; do
    for layout in ${DMG_LAYOUTS}; do
      TEMPLATE_NAME="dmg_${DMG_TEMPLATE_FORMAT}_${layout}.dmg"
      DMG_NAME="dmg_${format}_${layout}"
      hdiutil convert "${OUT_DIR}/${TEMPLATE_NAME}" \
        -format "${format}" -o "${OUT_DIR}/${DMG_NAME}"
    done
  done

  rm -rf "${DMG_SOURCE}"

  # DMG With Mach-O ############################################################

  mkdir "${DMG_SOURCE}"

  FAKE_APP="${DMG_SOURCE}/Foo.app/Contents/MacOS/"
  mkdir -p "${FAKE_APP}"
  cp "${THIS_DIR}/../mach_o/executablefat" "${FAKE_APP}"
  touch "${FAKE_APP}/../Info.plist"

  mkdir "${DMG_SOURCE}/.hidden"
  cp "${THIS_DIR}/../mach_o/lib64.dylib" "${DMG_SOURCE}/.hidden/"

  cp -r "${THIS_DIR}/../mach_o/shell-script.app" "${DMG_SOURCE}"

  hdiutil create -srcfolder "${DMG_SOURCE}" \
    -format UDZO -layout SPUD -volname "Mach-O in DMG" -ov \
    -fs JHFS+ "${OUT_DIR}/mach_o_in_dmg"

  rm -rf "${DMG_SOURCE}"

  # Copy of Mach-O DMG with 'koly' signature overwritten #######################
  cp "${OUT_DIR}/mach_o_in_dmg.dmg" \
      "${OUT_DIR}/mach_o_in_dmg_no_koly_signature.dmg"
  # Gets size of Mach-O DMG copy.
  SIZE=`stat -f%z "${OUT_DIR}/mach_o_in_dmg_no_koly_signature.dmg"`
  # Overwrites 'koly' with '????'.
  printf '\xa1\xa1\xa1\xa1' | dd conv=notrunc \
      of="${OUT_DIR}/mach_o_in_dmg_no_koly_signature.dmg" \
      bs=1 seek=$(($SIZE - 512)) &> /dev/null

  # Copy of Mach-O DMG with extension changed to .txt.
  cp "${OUT_DIR}/mach_o_in_dmg.dmg" "${OUT_DIR}/mach_o_in_dmg.txt"

  # Copy of Mach-O DMG with extension changed to .txt and no 'koly' signature ##
  cp "${OUT_DIR}/mach_o_in_dmg_no_koly_signature.dmg" \
      "${OUT_DIR}/mach_o_in_dmg_no_koly_signature.txt"

  # Copy of Mach-O DMG with partition Name/CFName overwritten ##################
  cp "${OUT_DIR}/mach_o_in_dmg.dmg" \
      "${OUT_DIR}/mach_o_in_dmg_no_partition_name.dmg"
  grep --byte-offset --only-matching --text Apple_HFS \
      "${OUT_DIR}/mach_o_in_dmg_no_partition_name.dmg" |
      sed 's/:.*$//' | while read -r match; do
      printf '         ' | dd conv=notrunc \
          of="${OUT_DIR}/mach_o_in_dmg_no_partition_name.dmg" \
          bs=1 seek=$match &> /dev/null
  done

  # Package Data for CIPD ######################################################

  cipd pkg-build -pkg-def "${THIS_DIR}/cipd.yaml" -out "${THIS_DIR}/data.zip"
}

# Silence any stdout, but keep stderr.
generate_test_data > /dev/null
