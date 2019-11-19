#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to generate symbols for a binary suitable for breakpad.

Currently, the tool only supports Linux, Android, and Mac. Support for other
platforms is planned.
"""

import collections
import errno
import glob
import multiprocessing
import optparse
import os
import Queue
import re
import shutil
import subprocess
import sys
import threading


CONCURRENT_TASKS=multiprocessing.cpu_count()

# The BINARY_INFO tuple describes a binary as dump_syms identifies it.
BINARY_INFO = collections.namedtuple('BINARY_INFO',
                                     ['platform', 'arch', 'hash', 'name'])


def GetDumpSymsBinary(build_dir=None):
  """Returns the path to the dump_syms binary."""
  DUMP_SYMS = 'dump_syms'
  dump_syms_bin = os.path.join(os.path.expanduser(build_dir), DUMP_SYMS)
  if not os.access(dump_syms_bin, os.X_OK):
    print 'Cannot find %s.' % dump_syms_bin
    return None

  return dump_syms_bin


def Resolve(path, exe_path, loader_path, rpaths):
  """Resolve a dyld path.

  @executable_path is replaced with |exe_path|
  @loader_path is replaced with |loader_path|
  @rpath is replaced with the first path in |rpaths| where the referenced file
      is found
  """
  path = path.replace('@loader_path', loader_path)
  path = path.replace('@executable_path', exe_path)
  if path.find('@rpath') != -1:
    for rpath in rpaths:
      new_path = path.replace('@rpath', rpath)
      if os.access(new_path, os.X_OK):
        return new_path
    return ''
  return path


def GetSharedLibraryDependenciesLinux(binary):
  """Return absolute paths to all shared library dependencies of the binary.

  This implementation assumes that we're running on a Linux system."""
  ldd = subprocess.check_output(['ldd', binary])
  lib_re = re.compile('\t.* => (.+) \(.*\)$')
  result = []
  for line in ldd.splitlines():
    m = lib_re.match(line)
    if m:
      result.append(os.path.abspath(m.group(1)))
  return result


def _GetSharedLibraryDependenciesAndroidOrChromeOS(binary):
  """GetSharedLibraryDependencies* suitable for Android or ChromeOS.

  Both assume that the host is Linux-based, but the binary being symbolized is
  being run on a device with potentially different architectures. Unlike ldd,
  readelf plays nice with mixed host/device architectures (e.g. x86-64 host,
  arm64 device), so use that.
  """
  readelf = subprocess.check_output(['readelf', '-d', binary])
  lib_re = re.compile('Shared library: \[(.+)\]$')
  result = []
  binary_path = os.path.dirname(os.path.abspath(binary))
  for line in readelf.splitlines():
    m = lib_re.search(line)
    if m:
      lib = os.path.join(binary_path, m.group(1))
      if os.access(lib, os.X_OK):
        result.append(lib)
  return result


def GetSharedLibraryDependenciesAndroid(binary):
  """Return absolute paths to all shared library dependencies of the binary.

  This implementation assumes that we're running on a Linux system, but
  compiled for Android."""
  return _GetSharedLibraryDependenciesAndroidOrChromeOS(binary)


def GetDeveloperDirMac():
  """Finds a good DEVELOPER_DIR value to run Mac dev tools.

  It checks the existing DEVELOPER_DIR and `xcode-select -p` and uses
  one of those if the folder exists, and falls back to one of the
  existing system folders with dev tools.

  Returns:
    (string) path to assign to DEVELOPER_DIR env var.
  """
  candidate_paths = []
  if 'DEVELOPER_DIR' in os.environ:
    candidate_paths.append(os.environ['DEVELOPER_DIR'])
  candidate_paths.extend([
    subprocess.check_output(['xcode-select', '-p']).strip(),
    # Most Mac 10.1[0-2] bots have at least one Xcode installed.
    '/Applications/Xcode.app',
    '/Applications/Xcode9.0.app',
    '/Applications/Xcode8.0.app',
    # Mac 10.13 bots don't have any Xcode installed, but have CLI tools as a
    # temporary workaround.
    '/Library/Developer/CommandLineTools',
  ])
  for path in candidate_paths:
    if os.path.exists(path):
      return path
  print 'WARNING: no value found for DEVELOPER_DIR. Some commands may fail.'


def GetSharedLibraryDependenciesMac(binary, exe_path):
  """Return absolute paths to all shared library dependencies of the binary.

  This implementation assumes that we're running on a Mac system."""
  # realpath() serves two purposes:
  # 1. If an executable is linked against a framework, it links against
  #    Framework.framework/Framework, which is a symlink to
  #    Framework.framework/Framework/Versions/A/Framework. rpaths are relative
  #    to the real location, so resolving the symlink is important.
  # 2. It converts binary to an absolute path. If binary is just
  #    "foo.dylib" in the current directory, dirname() would return an empty
  #    string, causing "@loader_path/foo" to incorrectly expand to "/foo".
  loader_path = os.path.dirname(os.path.realpath(binary))
  env = os.environ.copy()

  SRC_ROOT_PATH = os.path.join(os.path.dirname(__file__), '../../../..')
  hermetic_otool_path = os.path.join(
      SRC_ROOT_PATH, 'build', 'mac_files', 'xcode_binaries', 'Contents',
      'Developer', 'Toolchains', 'XcodeDefault.xctoolchain', 'usr', 'bin',
      'otool')
  if os.path.exists(hermetic_otool_path):
    otool_path = hermetic_otool_path
  else:
    developer_dir = GetDeveloperDirMac()
    if developer_dir:
      env['DEVELOPER_DIR'] = developer_dir
    otool_path = 'otool'

  otool = subprocess.check_output(
      [otool_path, '-l', binary], env=env).splitlines()
  rpaths = []
  dylib_id = None
  for idx, line in enumerate(otool):
    if line.find('cmd LC_RPATH') != -1:
      m = re.match(' *path (.*) \(offset .*\)$', otool[idx+2])
      rpath = m.group(1)
      rpath = rpath.replace('@loader_path', loader_path)
      rpath = rpath.replace('@executable_path', exe_path)
      rpaths.append(rpath)
    elif line.find('cmd LC_ID_DYLIB') != -1:
      m = re.match(' *name (.*) \(offset .*\)$', otool[idx+2])
      dylib_id = m.group(1)
  # `man dyld` says that @rpath is resolved against a stack of LC_RPATHs from
  # all executable images leading to the load of the current module. This is
  # intentionally not implemented here, since we require that every .dylib
  # contains all the rpaths it needs on its own, without relying on rpaths of
  # the loading executables.

  otool = subprocess.check_output(
      [otool_path, '-L', binary], env=env).splitlines()
  lib_re = re.compile('\t(.*) \(compatibility .*\)$')
  deps = []
  for line in otool:
    m = lib_re.match(line)
    if m:
      # For frameworks and shared libraries, `otool -L` prints the LC_ID_DYLIB
      # as the first line. Filter that out.
      if m.group(1) == dylib_id:
        continue
      dep = Resolve(m.group(1), exe_path, loader_path, rpaths)
      if dep:
        deps.append(os.path.normpath(dep))
      else:
        print >>sys.stderr, (
            'ERROR: failed to resolve %s, exe_path %s, loader_path %s, '
            'rpaths %s' % (m.group(1), exe_path, loader_path,
                           ', '.join(rpaths)))
        sys.exit(1)
  return deps


def GetSharedLibraryDependenciesChromeOS(binary):
  """Return absolute paths to all shared library dependencies of the binary.

  This implementation assumes that we're running on a Linux system, but
  compiled for ChromeOS."""
  return _GetSharedLibraryDependenciesAndroidOrChromeOS(binary)


def GetSharedLibraryDependencies(options, binary, exe_path):
  """Return absolute paths to all shared library dependencies of the binary."""
  deps = []
  if options.platform == 'linux2':
    deps = GetSharedLibraryDependenciesLinux(binary)
  elif options.platform == 'android':
    deps = GetSharedLibraryDependenciesAndroid(binary)
  elif options.platform == 'darwin':
    deps = GetSharedLibraryDependenciesMac(binary, exe_path)
  elif options.platform == 'chromeos':
    deps = GetSharedLibraryDependenciesChromeOS(binary)
  else:
    print "Platform not supported."
    sys.exit(1)

  result = []
  build_dir = os.path.abspath(options.build_dir)
  for dep in deps:
    if (os.access(dep, os.X_OK) and
        os.path.abspath(os.path.dirname(dep)).startswith(build_dir)):
      result.append(dep)
  return result


def GetTransitiveDependencies(options):
  """Return absolute paths to the transitive closure of all shared library
     dependencies of the binary, along with the binary itself."""
  binary = os.path.abspath(options.binary)
  exe_path = os.path.dirname(binary)
  if options.platform == 'linux2':
    # 'ldd' returns all transitive dependencies for us.
    deps = set(GetSharedLibraryDependencies(options, binary, exe_path))
    deps.add(binary)
    return list(deps)
  elif (options.platform == 'darwin' or options.platform == 'android' or
        options.platform == 'chromeos'):
    binaries = set([binary])
    queue = [binary]
    while queue:
      deps = GetSharedLibraryDependencies(options, queue.pop(0), exe_path)
      new_deps = set(deps) - binaries
      binaries |= new_deps
      queue.extend(list(new_deps))
    return binaries
  print "Platform not supported."
  sys.exit(1)


def mkdir_p(path):
  """Simulates mkdir -p."""
  try:
    os.makedirs(path)
  except OSError as e:
    if e.errno == errno.EEXIST and os.path.isdir(path):
      pass
    else: raise


def GetBinaryInfoFromHeaderInfo(header_info):
  """Given a standard symbol header information line, returns BINARY_INFO."""
  # header info is of the form "MODULE $PLATFORM $ARCH $HASH $BINARY"
  info_split = header_info.strip().split(' ', 4)
  if len(info_split) != 5 or info_split[0] != 'MODULE':
    return None
  return BINARY_INFO(*info_split[1:])


def CreateSymbolDir(options, output_dir, relative_hash_dir):
  """Create the directory to store breakpad symbols in. On Android/Linux, we
     also create a symlink in case the hash in the binary is missing."""
  mkdir_p(output_dir)
  if options.platform == 'android' or options.platform == "linux2":
    try:
      os.symlink(relative_hash_dir, os.path.join(os.path.dirname(output_dir),
                 '000000000000000000000000000000000'))
    except:
      pass


def GenerateSymbols(options, binaries):
  """Dumps the symbols of binary and places them in the given directory."""

  queue = Queue.Queue()
  print_lock = threading.Lock()

  def _Worker():
    dump_syms = GetDumpSymsBinary(options.build_dir)
    while True:
      should_dump_syms = True
      reason = "no reason"
      binary = queue.get()

      run_once = True
      while run_once:
        run_once = False

        if not dump_syms:
          should_dump_syms = False
          reason = "Could not locate dump_syms executable."
          break

        binary_info = GetBinaryInfoFromHeaderInfo(
            subprocess.check_output([dump_syms, '-i', binary]).splitlines()[0])
        if not binary_info:
          should_dump_syms = False
          reason = "Could not obtain binary information."
          break

        # See if the output file already exists.
        output_dir = os.path.join(options.symbols_dir, binary_info.name,
                                  binary_info.hash)
        output_path = os.path.join(output_dir, binary_info.name + '.sym')
        if os.path.isfile(output_path):
          should_dump_syms = False
          reason = "Symbol file already found."
          break

        # See if there is a symbol file already found next to the binary
        potential_symbol_files = glob.glob('%s.breakpad*' % binary)
        for potential_symbol_file in potential_symbol_files:
          with open(potential_symbol_file, 'rt') as f:
            symbol_info = GetBinaryInfoFromHeaderInfo(f.readline())
          if symbol_info == binary_info:
            CreateSymbolDir(options, output_dir, binary_info.hash)
            shutil.copyfile(potential_symbol_file, output_path)
            should_dump_syms = False
            reason = "Found local symbol file."
            break

      if not should_dump_syms:
        if options.verbose:
          with print_lock:
            print "Skipping %s (%s)" % (binary, reason)
        queue.task_done()
        continue

      if options.verbose:
        with print_lock:
          print "Generating symbols for %s" % binary

      CreateSymbolDir(options, output_dir, binary_info.hash)
      try:
        with open(output_path, 'wb') as f:
          subprocess.check_call([dump_syms, '-r', binary], stdout=f)
      except Exception, e:
        # Not much we can do about this.
        with print_lock:
          print e

      queue.task_done()

  for binary in binaries:
    queue.put(binary)

  for _ in range(options.jobs):
    t = threading.Thread(target=_Worker)
    t.daemon = True
    t.start()

  queue.join()


def main():
  parser = optparse.OptionParser()
  parser.add_option('', '--build-dir', default='',
                    help='The build output directory.')
  parser.add_option('', '--symbols-dir', default='',
                    help='The directory where to write the symbols file.')
  parser.add_option('', '--binary', default='',
                    help='The path of the binary to generate symbols for.')
  parser.add_option('', '--clear', default=False, action='store_true',
                    help='Clear the symbols directory before writing new '
                         'symbols.')
  parser.add_option('-j', '--jobs', default=CONCURRENT_TASKS, action='store',
                    type='int', help='Number of parallel tasks to run.')
  parser.add_option('-v', '--verbose', action='store_true',
                    help='Print verbose status output.')
  parser.add_option('', '--platform', default=sys.platform,
                    help='Target platform of the binary.')

  (options, _) = parser.parse_args()

  if not options.symbols_dir:
    print "Required option --symbols-dir missing."
    return 1

  if not options.build_dir:
    print "Required option --build-dir missing."
    return 1

  if not options.binary:
    print "Required option --binary missing."
    return 1

  if not os.access(options.binary, os.X_OK):
    print "Cannot find %s." % options.binary
    return 1

  if options.clear:
    try:
      shutil.rmtree(options.symbols_dir)
    except:
      pass

  if not GetDumpSymsBinary(options.build_dir):
    return 1

  # Build the transitive closure of all dependencies.
  binaries = GetTransitiveDependencies(options)

  GenerateSymbols(options, binaries)

  return 0


if '__main__' == __name__:
  sys.exit(main())
