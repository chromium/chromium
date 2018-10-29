#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script builds the credential provider installer that is used to install
# all required components of the Google Credential Provider for Windows.  The
# installer is a 7-zip self extracting executable file that wraps three main
# parts:
#
#  - the Credential Provider COM DLL
#  - a DLL that contains Windows EventLog message formatting
#  - a setup exe that performs action required during install and uninstall
#
# In this description "installer" refers to the self extracting executable that
# wraps all the parts, whereas "setup" refers to an exe inside the installer
# that runs specific actions at install and uninstall time.
#
# When run, the installer extracts the wrapped files into a new empty
# directory under %TEMP%.  The setup exe is then run to register the COM
# objects, install the message format dll, and properly register the credential
# provider with Windows.  Once installation completes, the new directory
# containing the extracted files is automatically deleted.
#
# The installer can be run multiple times on the same machine.  On an already
# working computer this is essentially a noop.  On a damaged computer the files
# will be overwritten and the parts registered, so can be used to correct
# problems.
#
# Running a new version of the installer will replace the existing install with
# a newer one.  It is not required to first uninstall the old version.
# Installation of the newer version will attempt to delete older versions if
# possible.
#
# The installer is not needed for uninstall and may be removed after initial
# install.  To uninstall the Google Credential Provider for Windows, run the
# setup exe with the command line argument: /uninstall

"""Creates the GCPW self extracting installer.  This script is not run manually,
it is called when building the //credential_provider:gcp_installer GN target.

All paths can be absolute or relative to $root_build_dir.
"""

import argparse
import os
import subprocess
import sys


def main():
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument('src_path', help='Path to the source root')
  parser.add_argument('cp_path',
                      help='Path to the credential provider directory')
  parser.add_argument('root_build_path', help='$root_build_dir GN variable')
  parser.add_argument('target_gen_path', help='$target_gen_dir GN variable')
  args = parser.parse_args()

  # Make sure all arguments are converted to absolute paths for use below.
  args.src_path = os.path.abspath(args.src_path)
  args.cp_path = os.path.abspath(args.cp_path)
  args.root_build_path = os.path.abspath(args.root_build_path)
  args.target_gen_path = os.path.abspath(args.target_gen_path)

  if not os.path.isdir(args.cp_path):
    parser.error('Invalid cp_path: "%s"' % args.cp_path)

  if not os.path.isdir(args.src_path):
    parser.error('Invalid src_path: "%s"' % args.src_path)

  # Absolute path to gcp installer.
  gcp_installer_fn = os.path.join(args.root_build_path, 'gcp_installer.exe')
  gcp_7z_fn = os.path.join(args.root_build_path, 'gcp.7z')

  sz_fn = os.path.join(args.src_path, r'third_party\lzma_sdk\7zr.exe')
  sfx_fn = os.path.join(args.root_build_path, 'gcp_sfx.exe')

  # Build the command line for updating files in the GCP 7z archive.
  cmd = [
      sz_fn,  # Path to 7z executable.
      'u',  # Update file in archive.

      # The follow options are equivalent to -mx9 with bcj2 turned on.
      # Because //third_party/lzma_sdk is only partial copy of the ful sdk
      # it does not support all forms of compression.  Make sure to use
      # compression that is compatible.  These same options are used when
      # building the chrome install compressed files.
      '-m0=BCJ2',
      '-m1=LZMA:d27:fb128',
      '-m2=LZMA:d22:fb128:mf=bt2',
      '-m3=LZMA:d22:fb128:mf=bt2',
      '-mb0:1',
      '-mb0s1:2',
      '-mb0s2:3',

      # Full path to archive.
      gcp_7z_fn,
  ]

  # Because of the way that 7zS2.sfx determine what program to run after
  # extraction, only gcp_setup.exe should be placed in the root of the archive.
  # Other "executable" type files (bat, cmd, exe, inf, msi, html, htm) should
  # be located only in subfolders.

  # Add the credential provider dll and setup programs to the archive.
  # If the files added to the archive are changed, make sure to update the
  # kFilenames array in setup_lib.cc.

  # 7zip and copy commands don't have a "silent" mode, so redirecting stdout
  # and stderr to nul.
  with open('nul') as nul_file:
    os.chdir(args.root_build_path)
    subprocess.check_call(cmd + ['gaia1_0.dll'], stdout=nul_file)
    subprocess.check_call(cmd + ['gcp_setup.exe'], stdout=nul_file)
    subprocess.check_call(cmd + ['gcp_eventlog_provider.dll'], stdout=nul_file)

  # Combine the SFX module with the archive to make a self extracting
  # executable.
  command = 'copy /b %s + %s %s > nul' % (sfx_fn, gcp_7z_fn, gcp_installer_fn)
  subprocess.check_call(command, shell=True)

  return 0


if __name__ == '__main__':
  sys.exit(main())
