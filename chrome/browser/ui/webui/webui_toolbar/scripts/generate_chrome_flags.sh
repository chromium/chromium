#! /bin/bash -e
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Output a C++ compatible multi-line string of flags. Paste this into
# chrome/browser/about_flags.cc and let clang-format take care of the rest.

# Split into lines on ",", sort them then add quotes around each line.
$(dirname "$0")/enabled.sh | \
    perl -lpe 's/,/,\n/g' | \
    sort | \
    perl -lpe 's/(^|$)/"/g'

# When enabled via chrome://flags, we want to enable the whole toolbar. It's OK
# that the flags above enable individual pieces.
echo '"WebUIToolar";'
