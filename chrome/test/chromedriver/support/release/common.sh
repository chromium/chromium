# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Common functions to be used by ChromeDriver publishing utilities


function ensure_linux {
  if [[ $(uname -s) != Linux* ]]
  then
    echo Please run $1 on Linux
    exit 1
  fi
}

function ensure_release_root {
  if [ ! -f '.version' ]
  then
    echo "File not found .version" >&2
    exit 1
  fi

  if [ ! -f '.type' ]
  then
    echo "File not found .type" >&2
    exit 1
  fi
}
