#!/bin/bash
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Example script that "installs" an app by writing some values to the install
# path.

declare appname=MockApp
declare company="Chromium"
declare product_version="1.0.0.0"
for i in "$@"; do
  case $i in
    --appname=*)
     appname="${i#*=}"
     ;;
    --company=*)
     company="${i#*=}"
     ;;
    --product_version=*)
     product_version="${i#*=}"
     ;;
    *)
      ;;
  esac
done

declare -r install_file="app.json"
if [[ "${OSTYPE}" =~ ^"darwin" ]]; then
  declare -r install_path="/Library/Application Support/${company}/${appname}"
else
  declare -r install_path="/opt/${company}/${appname}"
fi

mkdir -p "${install_path}"
cat << EOF > "${install_path}/${install_file}"
{
  "app": "${appname}",
  "company": "${company}",
  "pv": "${product_version}"
}
EOF

echo "Installed ${appname} version ${product_version} at: ${install_path}."
