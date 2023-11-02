# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

This zip file can be retrieved in one of 3 ways:
1) Download from cloud storage
2) Build zip_mini_installer_tests
3) Create one from an existing zip file

Running create_zip.py to distribute is the recommended way to distribute and
run the tests. This method creates a batch file that will automatically
set the paths.

vpython3 chrome\test\mini_installer\create_zip.py ^
  --output-path <ZIP_FILE_OUTPUT_PATH> ^
  --installer-path <CURRENT_INSTALLER_PATH> ^
  --previous-version-installer-path <PREVIOUS_INSTALLER_PATH> ^
  --chromedriver-path <CHROMEDRIVER_PATH>

create_zip.py is included in the zip files, so they are able to create new
zip files themselves. The drawback to this is the test cases aren't up to date
and chromedriver may be out of date.

The build target includes the chromedriver but no installers. There are
testers that do not have access to the source code, simply the installers.
Automatically adding 2 installers on build could cause confusion.

If you do build or download a fresh zip file its recommended to unzip,
put your installers in the directory, then use the command above to
create the final zip. These can be easily distributed to testers and easy
to maintain. When this method is used the --installer-path and
--previous-version-installer-path args must be passed to the batch file
since the default may be wrong.

To run:

1) Ensure the latest python 3 is installed and in your path
2) Unzip zip file
3) Open a command window in administrator mode
4) Navigate to the unzip folder
5) Run test_runner.bat file

Info:
The installer's filename is not changed during the zipping process so there is
no change to the way these are being identified now. Any args passed to the
batch file will be passed on to the test runner. -i, -p, and -c are
automatically set, but they can be overridden and all other flags can be set
if desired.

The batch script will use Python's pip module to install pywin32 and psutil
to reduce the amount of setup time is needed.
