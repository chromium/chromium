#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
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
import shutil
import subprocess
import sys


def GetLZMAExec(src_path):
  """Gets the path to the 7zip compression command line tool.

  Args:
    src_path: Full path to the source root

  Returns:
    The executable command to run the 7zip compressor.
  """
  executable = '7zr'
  if sys.platform == 'win32':
    executable += '.exe'

  return os.path.join(src_path, 'third_party', 'lzma_sdk', 'bin',
                      'host_platform', executable)

def GetCmdLine(command, sz_fn, gcp_7z_fn):
  """Builds the command line for the given archive.

  Args:
    command: 7Zip command such as 'u', 'rn'..
    sz_fn: The executable command to run 7zip.
    gcp_7z_fn: 7zip file for the archive.

  Returns:
    Returns the command line for the provided command and 7zip archive. Command
    needs to be one of the supported 7zip commands.
  """
  return [
      sz_fn,  # Path to 7z executable.
      command,

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

  sz_fn = GetLZMAExec(args.src_path)
  sfx_fn = os.path.join(args.root_build_path, 'gcp_sfx.exe')

  # Build the command line for updating files in the GCP 7z archive.
  u_cmd = GetCmdLine('u', sz_fn, gcp_7z_fn)

  # 7Zip CLI doesn't provide a direct way of adding files into a custom
  # folder. As suggested by the developer of 7Zip the method is to rename
  # a file with a specific subfolder to achieve the same.
  # https://sourceforge.net/p/sevenzip/discussion/45798/thread/5856d980/
  # For instance, if there is a file called "a.txt" in the parent folder of
  # archive, when it is renamed with "f\a.txt", the "a.txt" file is actually
  # placed in a folder called "f".
  rn_cmd = GetCmdLine('rn', sz_fn, gcp_7z_fn)

  # Builds the command line for deleting files in the archive.
  d_cmd = GetCmdLine('d', sz_fn, gcp_7z_fn)

  # Because of the way that 7zS2.sfx determine what program to run after
  # extraction, only gcp_setup.exe should be placed in the root of the archive.
  # Other "executable" type files (bat, cmd, exe, inf, msi, html, htm) should
  # be located only in subfolders. That's why all the files initially added in
  # the top folder. Then the ones need to move to subfolders are renamed. 7z
  # doesn't have a method to achieve the same directly.

  # Add the credential provider dll, credential provider extension and setup
  # programs to the archive. If the files added to the archive are changed,
  # make sure to update the kFilenames array in setup_lib.cc.

  try:
    gcpw_log_file = 'gcpw_archive_log_file'
    if os.path.exists(gcpw_log_file):
      os.remove(gcpw_log_file)

    # Redirecting output of 7zip and copy commands to a file and only printing
    # if any of the subprocess commands fail.
    with open(gcpw_log_file, "w+") as output_file:
      os.chdir(args.root_build_path)
      subprocess.check_call(d_cmd + ['*'], stdout=output_file)
      subprocess.check_call(u_cmd + ['gaia1_0.dll'], stdout=output_file)
      subprocess.check_call(u_cmd + ['gcp_setup.exe'], stdout=output_file)
      subprocess.check_call(u_cmd + ['gcp_eventlog_provider.dll'],
          stdout=output_file)
      subprocess.check_call(u_cmd + ['gcpw_extension.exe'], stdout=output_file)
      # Move the executable into a subfolder as there needs to be only one
      # executable in the parent folder.
      subprocess.check_call(rn_cmd +
          [
            'gcpw_extension.exe',
            os.path.join('extension', 'gcpw_extension.exe')
          ],
          stdout=output_file)
  except subprocess.CalledProcessError as e:
    print(e.output)
    with open(gcpw_log_file, "r") as output_file:
      print(output_file.read())
    raise e

  # Combine the SFX module with the archive to make a self extracting
  # executable.
  with open(gcp_installer_fn, 'wb') as output:
    with open (sfx_fn, 'rb') as input:
      shutil.copyfileobj(input, output)
    with open (gcp_7z_fn, 'rb') as input:
      shutil.copyfileobj(input, output)

  return 0


if __name__ == '__main__':
  sys.exit(main())
