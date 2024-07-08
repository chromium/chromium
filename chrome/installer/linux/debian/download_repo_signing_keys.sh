#!/bin/bash
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o nounset
set -o errexit

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

KEY_FINGERS=(
  # Debian Archive Automatic Signing Key (7.0/wheezy)
  "A1BD8E9D78F7FE5C3E65D8AF8B48AD6246925553"
  # Jessie Stable Release Key
  "75DDC3C4A499F1A18CB5F3C8CBF8D6FD518E17E1"
  # Debian Archive Automatic Signing Key (8/jessie)
  "126C0D24BD8A2942CC7DF8AC7638D0442B90D010"
  # Debian Security Archive Automatic Signing Key (8/jessie)
  "D21169141CECD440F2EB8DDA9D6D8F6BC857C906"
  # Debian Archive Automatic Signing Key (9/stretch)
  "16E90B3FDF65EDE3AA7F323C04EE7237B7D453EC"
  # Debian Security Archive Automatic Signing Key (9/stretch)
  "379483D8B60160B155B372DDAA8E81B4331F7F50"
  # Debian Stable Release Key (9/stretch)
  "067E3C456BAE240ACEE88F6FEF0F382A1A7B6500"
  # Debian Stable Release Key (10/buster)
  "6D33866EDD8FFA41C0143AEDDCC9EFBF77E11517"
  # Debian Archive Automatic Signing Key (10/buster)
  "0146DC6D4A0B2914BDED34DB648ACFD622F3D138"
  # Debian Security Archive Automatic Signing Key (10/buster)
  "5237CEEEF212F3D51C74ABE0112695A0E562B32A"
  # Ubuntu Archive Automatic Signing Key
  "630239CC130E1A7FD81A27B140976EAF437D05B5"
  # Ubuntu Archive Automatic Signing Key (2012)
  "790BC7277767219C42C86F933B4FE6ACC0B21F32"
  # Ubuntu Archive Automatic Signing Key (2018)
  "F6ECB3762474EDA9D21B7022871920D1991BC93C"
  # Debian Archive Automatic Signing Key (11/bullseye)
  "A7236886F3CCCAAD148A27F80E98404D386FA1D9"
  # Debian Security Archive Automatic Signing Key (11/bullseye)
  "ED541312A33F1128F10B1C6C54404762BBB6E853"
  # Debian Archive Automatic Signing Key (12/bookworm)
  "4CB50190207B4758A3F73A796ED0E7B82643E131"
  # Debian Stable Release Key (12/bookworm)
  "4D64FEC119C2029067D6E791F8D2585B8783D481"
  # Debian Security Archive Automatic Signing Key (12/bookworm)
  "B0CAB9266E8C3929798B3EEEBDE6D2B9216EC7A8"
  # Debian Stable Release Key (11/bullseye)
  "A4285295FC7B1A81600062A9605C66F00D6C9793"
)

gpg --keyserver keyserver.ubuntu.com --recv-keys ${KEY_FINGERS[@]}
gpg --output "${SCRIPT_DIR}/repo_signing_keys.gpg" --export ${KEY_FINGERS[@]}
