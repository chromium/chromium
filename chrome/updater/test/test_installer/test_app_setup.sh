#!/bin/bash
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Example script that "installs" an app by writing some values to the install
# path.

declare appid=MockApp
declare system=0
declare company="Chromium"
declare product_version="1.0.0.0"
declare install=1
for i in "$@"; do
  case $i in
    --appid=*)
     appid="${i#*=}"
     ;;
    --system)
     system=1
     ;;
    --company=*)
     company="${i#*=}"
     ;;
    --product_version=*)
     product_version="${i#*=}"
     ;;
    --uninstall)
     install=0
     ;;
    *)
      ;;
  esac
done

declare -r install_file="app.json"
if [[ "${OSTYPE}" =~ ^"darwin" ]]; then
  if (( "${system}" == 1 )); then
    declare -r install_path="/Library/Application Support/${company}/${appid}"
  else
    declare -r \
        install_path="${HOME}/Library/Application Support/${company}/${appid}"
  fi
else
  declare -r lower_company=$(echo ${company} | tr '[:upper:]' '[:lower:]')
  if (( "${system}" == 1 )); then
    declare -r install_path="/opt/${lower_company}/${appid}"
  else
    declare -r install_path="${HOME}/.local/${lower_company}/${appid}"
  fi
fi

if (( "${install}" == 1 )); then
  mkdir -p "${install_path}"
  cat << EOF > "${install_path}/${install_file}"
  {
    "app": "${appid}",
    "company": "${company}",
    "pv": "${product_version}"
  }
EOF

  echo "Installed ${appid} version ${product_version} at: ${install_path}."
else
  rm -rf "${install_path}" 2> /dev/null
  echo "Uninstall ${appid} at: ${install_path}."
fi
