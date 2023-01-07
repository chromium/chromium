#!/usr/bin/env python
#
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Links the deps of a binary into a static library.

Run with a working directory, the name of a binary target, and the name of the
static library that should be produced. For example:

  $ link_dependencies.py out/Release-iphoneos \
                         crnet_consumer.app/crnet_consumer \
                         out/Release-iphoneos/crnet_standalone.a
"""

import argparse
import os
import re
import subprocess
import sys


class SubprocessError(Exception):
  pass


def extract_inputs(query_result, prefix=''):
  """Extracts inputs from ninja query output.

  Given 'ninja -t query' output for a target, extracts all the inputs of that
  target, prefixing them with an optional prefix. Inputs prefixed with '|' are
  implicit, so we discard them as they shouldn't be linked into the resulting
  binary (these are things like the .ninja files themselves, dep lists, and so
  on).

  Example query result:
    arch/crnet_consumer.armv7:
      input: link
        obj/[long path...]/crnet_consumer.crnet_consumer_app_delegate.armv7.o
        obj/[long path...]/crnet_consumer.crnet_consumer_view_controller.armv7.o
        obj/[long path...]/crnet_consumer.main.armv7.o
        libcrnet.a
        libdata_reduction_proxy_code_browser.a
        ... many more inputs ...
        liburl_util.a
        | obj/content/content.actions_depends.stamp
        | gen/components/data_reduction_proxy/common/version.h
        | obj/ui/resources/ui_resources.actions_rules_copies.stamp
        ... more implicit inputs ...
    outputs:
      crnet_consumer.app/crnet_consumer

  Args:
    query_result: output from 'ninja -t query'
    prefix: optional file system path to prefix to returned inputs

  Returns:
    A list of the inputs.
  """
  extracting = False
  inputs = []
  for line in query_result.splitlines():
    if line.startswith('  input:'):
      extracting = True
    elif line.startswith('  outputs:'):
      extracting = False
    elif extracting and '|' not in line:
      inputs.append(os.path.join(prefix, line.strip()))
  return inputs


def query_ninja(target, workdir, prefix=''):
  """Returns the inputs for the named target.

  Queries ninja for the set of inputs of the named target, then returns the list
  of inputs to that target.

  Args:
    target: ninja target name to query for
    workdir: workdir for ninja
    prefix: optional file system path to prefix to returned inputs

  Returns:
    A list of file system paths to the inputs to the named target.
  """
  proc = subprocess.Popen(['ninja', '-C', workdir, '-t', 'query', target],
                          stdout=subprocess.PIPE)
  stdout, _ = proc.communicate()
  return extract_inputs(stdout, prefix)


def is_library(target):
  """Returns whether target is a library file."""
  return os.path.splitext(target)[1] in ('.a', '.o')


def library_deps(targets, workdir, query=query_ninja):
  """Returns the set of library dependencies for the supplied targets.

  The entries in the targets list can be either a static library, an object
  file, or an executable. Static libraries and object files are incorporated
  directly; executables are treated as being thin executable inputs to a fat
  executable link step, and have their own library dependencies added in their
  place.

  Args:
    targets: list of targets to include library dependencies from
    workdir: working directory to run ninja queries in
    query: function taking target, workdir, and prefix and returning an input
           set
  Returns:
    Set of library dependencies.
  """
  deps = set()
  for target in targets:
    if is_library(target):
      deps.add(os.path.join(workdir, target))
    else:
      deps = deps.union(query(target, workdir, workdir))
  return deps


def link(output, inputs):
  """Links output from inputs using libtool.

  Args:
    output: file system path to desired output library
    inputs: list of file system paths to input libraries
  """
  libtool_re = re.compile(r'^.*libtool: (?:for architecture: \S* )?'
                          r'file: .* has no symbols$')
  p = subprocess.Popen(
      ['libtool', '-o', output] + inputs, stderr=subprocess.PIPE)
  _, err = p.communicate()
  for line in err.splitlines():
    if not libtool_re.match(line):
      sys.stderr.write(line)
  if p.returncode != 0:
    message = "subprocess libtool returned {0}".format(p.returncode)
    raise SubprocessError(message)


def main():
  parser = argparse.ArgumentParser(
      description='Link dependencies of a ninja target into a static library')
  parser.add_argument('workdir', nargs=1, help='ninja working directory')
  parser.add_argument('target', nargs=1, help='target to query for deps')
  parser.add_argument('output', nargs=1, help='path to output static library')
  args = parser.parse_args()

  inputs = query_ninja(args.target[0], args.workdir[0])
  link(args.output[0], list(library_deps(inputs, args.workdir[0])))


if __name__ == '__main__':
  main()
