#!/usr/bin/env python
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
 Converts a given ASCII proto into a binary resource.

"""
from __future__ import print_function
import abc
from importlib import util as imp_util
import optparse
import os
import re
import subprocess
import sys
import traceback


class GoogleProtobufModuleImporter:
  """A custom module importer for importing google.protobuf.

  See PEP #302 (https://www.python.org/dev/peps/pep-0302/) for full information
  on the Importer Protocol.
  """

  def __init__(self, paths):
    """Creates a loader that searches |paths| for google.protobuf modules."""
    self._paths = paths

  def _fullname_to_filepath(self, fullname):
    """Converts a full module name to a corresponding path to a .py file.

    e.g. google.protobuf.text_format -> pyproto/google/protobuf/text_format.py
    """
    for path in self._paths:
      filepath = os.path.join(path, fullname.replace('.', os.sep) + '.py')
      if os.path.isfile(filepath):
        return filepath
    return None

  def _module_exists(self, fullname):
    return self._fullname_to_filepath(fullname) is not None

  def find_module(self, fullname, path=None):
    """Returns a loader module for the google.protobuf module in pyproto."""
    if (fullname.startswith('google.protobuf.')
        and self._module_exists(fullname)):
      # Per PEP #302, this will result in self.load_module getting used
      # to load |fullname|.
      return self

    # Per PEP #302, if the module cannot be loaded, then return None.
    return None

  def load_module(self, fullname):
    """Loads the module specified by |fullname| and returns the module."""
    if fullname in sys.modules:
      # Per PEP #302, if |fullname| is in sys.modules, it must be returned.
      return sys.modules[fullname]

    if (not fullname.startswith('google.protobuf.') or
        not self._module_exists(fullname)):
      # Per PEP #302, raise ImportError if the requested module/package
      # cannot be loaded. This should never get reached for this simple loader,
      # but is included for completeness.
      raise ImportError(fullname)

    filepath = self._fullname_to_filepath(fullname)
    spec = imp_util.spec_from_file_location(fullname, filepath)
    loaded = imp_util.module_from_spec(spec)
    spec.loader.exec_module(loaded)

    return loaded

class BinaryProtoGenerator:

  # If the script is run in a virtualenv
  # (https://virtualenv.pypa.io/en/stable/), then no google.protobuf library
  # should be brought in from site-packages. Passing -S into the interpreter in
  # a virtualenv actually destroys the ability to import standard library
  # functions like optparse, so this script should not be wrapped if we're in a
  # virtualenv.
  def _IsInVirtualEnv(self):
    # This is the way used by pip and other software to detect virtualenv.
    return hasattr(sys, 'real_prefix')

  def _ImportProtoModules(self, paths):
    """Import the protobuf modules we need. |paths| is list of import paths"""
    for path in paths:
      # Put the path to our proto libraries in front, so that we don't use
      # system protobuf.
      sys.path.insert(1, path)

    if self._IsInVirtualEnv():
      # Add a custom module loader. When run in a virtualenv that has
      # google.protobuf installed, the site-package was getting searched first
      # despite that pyproto/ is at the start of the sys.path. The module
      # loaders in the meta_path precede all other imports (including even
      # builtins), which allows the proper google.protobuf from pyproto to be
      # found.
      sys.meta_path.append(GoogleProtobufModuleImporter(paths))

    import google.protobuf.text_format as text_format
    globals()['text_format'] = text_format
    self.ImportProtoModule()

  def _GenerateBinaryProtos(self, opts):
    """ Read the ASCII proto and generate one or more binary protos. """
    # Read the ASCII
    with open(opts.infile, 'r') as ifile:
      ascii_pb_str = ifile.read()

    # Parse it into a structured PB
    full_pb = self.EmptyProtoInstance()
    text_format.Merge(ascii_pb_str, full_pb)

    self.ValidatePb(opts, full_pb);
    self.ProcessPb(opts, full_pb)

  @abc.abstractmethod
  def ImportProtoModule(self):
    """ Import the proto module to be used by the generator. """
    pass

  @abc.abstractmethod
  def EmptyProtoInstance(self):
    """ Returns an empty proto instance to be filled by the generator."""
    pass

  @abc.abstractmethod
  def ValidatePb(self, opts, pb):
    """ Validate the basic values of the protobuf.  The
        file_type_policies_unittest.cc will also validate it by platform,
        but this will catch errors earlier.
    """
    pass

  @abc.abstractmethod
  def ProcessPb(self, opts, pb):
    """ Process the parsed prototobuf. """
    pass

  def AddCommandLineOptions(self, parser):
    """ Allows subclasses to add any options the command line parser. """
    pass

  def AddExtraCommandLineArgsForVirtualEnvRun(self, opts, command):
    """ Allows subclasses to add any extra command line arguments when running
        this under a virtualenv."""
    pass

  def VerifyArgs(self, opts):
    """ Allows subclasses to check command line parameters before running. """
    return True

  def Run(self):
    parser = optparse.OptionParser()
    # TODO(crbug.com/41255210): Remove this once the bug is fixed.
    parser.add_option('-w', '--wrap', action="store_true", default=False,
                      help='Wrap this script in another python '
                      'execution to disable site-packages.  This is a '
                      'fix for http://crbug.com/605592')

    parser.add_option('-i', '--infile',
                      help='The ASCII proto file to read.')
    parser.add_option('-d', '--outdir',
                      help='Directory underwhich binary file(s) will be ' +
                           'written')
    parser.add_option('-o', '--outbasename',
                      help='Basename of the binary file to write to.')
    parser.add_option('-p', '--path', action="append",
                      help='Repeat this as needed.  Directory(s) containing ' +
                      'the your_proto_definition_pb2.py and ' +
                      'google.protobuf.text_format modules')
    self.AddCommandLineOptions(parser)

    (opts, args) = parser.parse_args()
    if opts.infile is None or opts.outdir is None or opts.outbasename is None:
      parser.print_help()
      return 1

    if opts.wrap and not self._IsInVirtualEnv():
      # Run this script again with different args to the interpreter to suppress
      # the inclusion of libraries, like google.protobuf, from site-packages,
      # which is checked before sys.path when resolving imports. We want to
      # specifically import the libraries injected into the sys.path in
      # ImportProtoModules().
      command = [sys.executable, '-S', '-s', sys.argv[0]]
      command += ['-i', opts.infile]
      command += ['-d', opts.outdir]
      command += ['-o', opts.outbasename]
      for path in opts.path:
        command += ['-p', path]

      self.AddExtraCommandLineArgsForVirtualEnvRun(opts, command);
      sys.exit(subprocess.call(command))

    self._ImportProtoModules(opts.path)

    if not self.VerifyArgs(opts):
      print("Wrong arguments")
      return 1

    try:
      self._GenerateBinaryProtos(opts)
    except Exception as e:
      print("ERROR: Failed to render binary version of %s:\n  %s\n%s" %
            (opts.infile, str(e), traceback.format_exc()))
      return 1
