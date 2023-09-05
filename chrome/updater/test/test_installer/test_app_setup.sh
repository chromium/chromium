#!/bin/bash
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Example script that "installs" an app by writing some values to the install
# path.

declare appid="{AE098195-B8DB-4A49-8E23-84FCACB61FF1}"
declare system=0
declare company="Google"
declare production_version="1.0.0.0"
declare install_path="install_result.txt"

for i in "$@"; do
  case $i in
    --system)
      system=1
      ;;
    --appid=*)
     appid="${i#*=}"
     ;;
    --company=*)
     company="${i#*=}"
     ;;
    --product_version=*)
     product_version="${i#*=}"
     ;;
    --install_path=*)
     install_path="${i#*=}"
     ;;
    *)
      ;;
  esac
done

mkdir -p $(dirname ${install_path})
cat << EOF > ${install_path}
system=${system}
appid=${appid}
company=${company}
product_version=${production_version}
EOF
