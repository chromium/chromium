This directory contains code needed to validate and assemble chrome packages for
deployment. The main file in each platform-specific subdirectory is `FILES.cfg`.
This file is interpreted by python's execfile, and the global FILES is
extracted. For more details on how this file is used, see the [extraction
logic](https://source.chromium.org/chromium/chromium/tools/build/+/main:scripts/common/archive_utils.py).
