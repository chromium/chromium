#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A utility script that can extract and edit resources in a Windows binary.

For detailed help, see the script's usage by invoking it with --help."""

import ctypes
import ctypes.wintypes
import logging
import optparse
import os
import shutil
import sys
import tempfile
import win32api
import win32con


_LOGGER = logging.getLogger(__name__)


# The win32api-supplied UpdateResource wrapper unfortunately does not allow
# one to remove resources due to overzealous parameter verification.
# For that case we're forced to go straight to the native API implementation.
UpdateResource = ctypes.windll.kernel32.UpdateResourceW
UpdateResource.argtypes = [
    ctypes.wintypes.HANDLE,  # HANDLE hUpdate
    ctypes.c_wchar_p,  # LPCTSTR lpType
    ctypes.c_wchar_p,  # LPCTSTR lpName
    ctypes.c_short,  # WORD wLanguage
    ctypes.c_void_p,  # LPVOID lpData
    ctypes.c_ulong,  # DWORD cbData
    ]
UpdateResource.restype = ctypes.c_short


def _ResIdToString(res_id):
  # Convert integral res types/ids to a string.
  if isinstance(res_id, int):
    return "#%d" % res_id

  return res_id


class ResourceEditor(object):
  """A utility class to make it easy to extract and manipulate resources in a
  Windows binary."""

  def __init__(self, input_file, output_file):
    """Create a new editor.

    Args:
        input_file: path to the input file.
        output_file: (optional) path to the output file.
    """
    self._input_file = input_file
    self._output_file = output_file
    self._modified = False
    self._module = None
    self._temp_dir = None
    self._temp_file = None
    self._update_handle = None

  def __del__(self):
    if self._module:
      win32api.FreeLibrary(self._module)
      self._module = None

    if self._update_handle:
      _LOGGER.info('Canceling edits to "%s".', self.input_file)
      win32api.EndUpdateResource(self._update_handle, False)
      self._update_handle = None

    if self._temp_dir:
      _LOGGER.info('Removing temporary directory "%s".', self._temp_dir)
      shutil.rmtree(self._temp_dir)
      self._temp_dir = None

  def _GetModule(self):
    if not self._module:
      # Specify a full path to LoadLibraryEx to prevent
      # it from searching the path.
      input_file = os.path.abspath(self.input_file)
      _LOGGER.info('Loading input_file from "%s"', input_file)
      self._module = win32api.LoadLibraryEx(
          input_file, None, win32con.LOAD_LIBRARY_AS_DATAFILE)
    return self._module

  def _GetTempDir(self):
    if not self._temp_dir:
      self._temp_dir = tempfile.mkdtemp()
      _LOGGER.info('Created temporary directory "%s".', self._temp_dir)

    return self._temp_dir

  def _GetUpdateHandle(self):
    if not self._update_handle:
      # Make a copy of the input file in the temp dir.
      self._temp_file = os.path.join(self.temp_dir,
                                     os.path.basename(self._input_file))
      shutil.copyfile(self._input_file, self._temp_file)
      # Open a resource update handle on the copy.
      _LOGGER.info('Opening temp file "%s".', self._temp_file)
      self._update_handle = win32api.BeginUpdateResource(self._temp_file, False)

    return self._update_handle

  modified = property(lambda self: self._modified)
  input_file = property(lambda self: self._input_file)
  module = property(_GetModule)
  temp_dir = property(_GetTempDir)
  update_handle = property(_GetUpdateHandle)

  def ExtractAllToDir(self, extract_to):
    """Extracts all resources from our input file to a directory hierarchy
    in the directory named extract_to.

    The generated directory hierarchy is three-level, and looks like:
      resource-type/
        resource-name/
          lang-id.

    Args:
      extract_to: path to the folder to output to. This folder will be erased
          and recreated if it already exists.
    """
    _LOGGER.info('Extracting all resources from "%s" to directory "%s".',
        self.input_file, extract_to)

    if os.path.exists(extract_to):
      _LOGGER.info('Destination directory "%s" exists, deleting', extract_to)
      shutil.rmtree(extract_to)

    # Make sure the destination dir exists.
    os.makedirs(extract_to)

    # Now enumerate the resource types.
    for res_type in win32api.EnumResourceTypes(self.module):
      res_type_str = _ResIdToString(res_type)

      # And the resource names.
      for res_name in win32api.EnumResourceNames(self.module, res_type):
        res_name_str = _ResIdToString(res_name)

        # Then the languages.
        for res_lang in win32api.EnumResourceLanguages(self.module,
            res_type, res_name):
          res_lang_str = _ResIdToString(res_lang)

          dest_dir = os.path.join(extract_to, res_type_str, res_lang_str)
          dest_file = os.path.join(dest_dir, res_name_str)
          _LOGGER.info('Extracting resource "%s", lang "%d" name "%s" '
                       'to file "%s".',
                       res_type_str, res_lang, res_name_str, dest_file)

          # Extract each resource to a file in the output dir.
          if not os.path.exists(dest_dir):
            os.makedirs(dest_dir)
          self.ExtractResource(res_type, res_lang, res_name, dest_file)

  def ExtractResource(self, res_type, res_lang, res_name, dest_file):
    """Extracts a given resource, specified by type, language id and name,
    to a given file.

    Args:
      res_type: the type of the resource, e.g. "B7".
      res_lang: the language id of the resource e.g. 1033.
      res_name: the name of the resource, e.g. "SETUP.EXE".
      dest_file: path to the file where the resource data will be written.
    """
    _LOGGER.info('Extracting resource "%s", lang "%d" name "%s" '
                 'to file "%s".', res_type, res_lang, res_name, dest_file)

    data = win32api.LoadResource(self.module, res_type, res_name, res_lang)
    with open(dest_file, 'wb') as f:
      f.write(data)

  def RemoveResource(self, res_type, res_lang, res_name):
    """Removes a given resource, specified by type, language id and name.

    Args:
      res_type: the type of the resource, e.g. "B7".
      res_lang: the language id of the resource, e.g. 1033.
      res_name: the name of the resource, e.g. "SETUP.EXE".
    """
    _LOGGER.info('Removing resource "%s:%s".', res_type, res_name)
    # We have to go native to perform a removal.
    ret = UpdateResource(self.update_handle,
                         res_type,
                         res_name,
                         res_lang,
                         None,
                         0)
    # Raise an error on failure.
    if ret == 0:
      error = win32api.GetLastError()
      print ("error", error)
      raise RuntimeError(error)
    self._modified = True

  def UpdateResource(self, res_type, res_lang, res_name, file_path):
    """Inserts or updates a given resource with the contents of a file.

    This is a legacy version of UpdateResourceData, where the data arg is read
    from a file , rather than passed directly.
    """
    _LOGGER.info('Writing resource from file %s', file_path)
    with open(file_path, 'rb') as f:
      self.UpdateResourceData(res_type, res_lang, res_name, f.read())

  def UpdateResourceData(self, res_type, res_lang, res_name, data):
    """Inserts or updates a given resource with the given data.

    Args:
      res_type: the type of the resource, e.g. "B7".
      res_lang: the language id of the resource, e.g. 1033.
      res_name: the name of the resource, e.g. "SETUP.EXE".
      data: the new resource data.
    """
    _LOGGER.info('Writing resource "%s:%s"', res_type, res_name)
    win32api.UpdateResource(self.update_handle,
                            res_type,
                            res_name,
                            data,
                            res_lang)
    self._modified = True

  def Commit(self):
    """Commit any successful resource edits this editor has performed.

    This has the effect of writing the output file.
    """
    if self._update_handle:
      update_handle = self._update_handle
      self._update_handle = None
      win32api.EndUpdateResource(update_handle, False)
      _LOGGER.info('Writing edited file to "%s".', self._output_file)
      shutil.copyfile(self._temp_file, self._output_file)
    else:
      _LOGGER.info('No edits made. Copying input to "%s".', self._output_file)
      shutil.copyfile(self._input_file, self._output_file)


_USAGE = """\
usage: %prog [options] input_file

A utility script to extract and edit the resources in a Windows executable.

EXAMPLE USAGE:
# Extract from mini_installer.exe, the resource type "B7", langid 1033 and
# name "CHROME.PACKED.7Z" to a file named chrome.7z.
# Note that 1033 corresponds to English (United States).
%prog mini_installer.exe --extract B7 1033 CHROME.PACKED.7Z chrome.7z

# Update mini_installer.exe by removing the resouce type "BL", langid 1033 and
# name "SETUP.EXE". Add the resource type "B7", langid 1033 and name
# "SETUP.EXE.packed.7z" from the file setup.packed.7z.
# Write the edited file to mini_installer_packed.exe.
%prog mini_installer.exe \\
    --remove BL 1033 SETUP.EXE \\
    --update B7 1033 SETUP.EXE.packed.7z setup.packed.7z \\
    --output_file mini_installer_packed.exe
"""

def _ParseArgs():
  parser = optparse.OptionParser(_USAGE)
  parser.add_option('--verbose', action='store_true',
      help='Enable verbose logging.')
  parser.add_option('--extract_all',
      help='Path to a folder which will be created, in which all resources '
           'from the input_file will be stored, each in a file named '
           '"res_type/lang_id/res_name".')
  parser.add_option('--extract', action='append', default=[], nargs=4,
      help='Extract the resource with the given type, language id and name '
           'to the given file.',
      metavar='type langid name file_path')
  parser.add_option('--remove', action='append', default=[], nargs=3,
      help='Remove the resource with the given type, langid and name.',
      metavar='type langid name')
  parser.add_option('--update', action='append', default=[], nargs=4,
      help='Insert or update the resource with the given type, langid and '
           'name with the contents of the file given.',
      metavar='type langid name file_path')
  parser.add_option('--output_file',
    help='On success, OUTPUT_FILE will be written with a copy of the '
         'input file with the edits specified by any remove or update '
         'options.')

  options, args = parser.parse_args()

  if len(args) != 1:
    parser.error('You have to specify an input file to work on.')

  modify = options.remove or options.update
  if modify and not options.output_file:
    parser.error('You have to specify an output file with edit options.')

  return options, args


def _ConvertInts(*args):
  """Return args with any all-digit strings converted to ints."""
  results = []
  for arg in args:
    if isinstance(arg, basestring) and arg.isdigit():
      results.append(int(arg))
    else:
      results.append(arg)
  return results


def main(options, args):
  """Main program for the script."""
  if options.verbose:
    logging.basicConfig(level=logging.INFO)

  # Create the editor for our input file.
  editor = ResourceEditor(args[0], options.output_file)

  if options.extract_all:
    editor.ExtractAllToDir(options.extract_all)

  for res_type, res_lang, res_name, dest_file in options.extract:
    res_type, res_lang, res_name = _ConvertInts(res_type, res_lang, res_name)
    editor.ExtractResource(res_type, res_lang, res_name, dest_file)

  for res_type, res_lang, res_name in options.remove:
    res_type, res_lang, res_name = _ConvertInts(res_type, res_lang, res_name)
    editor.RemoveResource(res_type, res_lang, res_name)

  for res_type, res_lang, res_name, src_file in options.update:
    res_type, res_lang, res_name = _ConvertInts(res_type, res_lang, res_name)
    editor.UpdateResource(res_type, res_lang, res_name, src_file)

  if editor.modified:
    editor.Commit()


if __name__ == '__main__':
  sys.exit(main(*_ParseArgs()))
