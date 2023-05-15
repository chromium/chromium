#!/usr/bin/env python
#
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import itertools
import os
import shutil
import sys
import tempfile

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

sys.path.insert(0, os.path.join(REPOSITORY_ROOT, 'build/android/gyp'))
sys.path.insert(0, os.path.join(REPOSITORY_ROOT, 'net/tools/net_docs'))
# pylint: disable=wrong-import-position
from util import build_utils
import action_helpers  # build_utils adds //build to sys.path.
import net_docs
from markdown.postprocessors import Postprocessor
from markdown.extensions import Extension
# pylint: enable=wrong-import-position

DOCLAVA_DIR = os.path.join(REPOSITORY_ROOT, 'buildtools', 'android', 'doclava')
SDK_DIR = os.path.join(REPOSITORY_ROOT, 'third_party', 'android_sdk', 'public')
# TODO(b/260694901) Remove this usage of Java 11 as soon as Doclava supports it.
# Doclava support for JDK17 was actively being worked on as of Jan 2023.
JAVA_11_HOME = os.path.join(REPOSITORY_ROOT, 'third_party', 'jdk11', 'current')
JAVADOC_PATH = os.path.join(JAVA_11_HOME, 'bin', 'javadoc')
JAR_PATH = os.path.join(JAVA_11_HOME, 'bin', 'jar')

JAVADOC_WARNING = """\
javadoc: warning - The old Doclet and Taglet APIs in the packages
com.sun.javadoc, com.sun.tools.doclets and their implementations
are planned to be removed in a future JDK release. These
components have been superseded by the new APIs in jdk.javadoc.doclet.
Users are strongly recommended to migrate to the new APIs.
"""

class CronetPostprocessor(Postprocessor):
  def run(self, text):
    return text.replace('@Override', '&commat;Override')


class CronetExtension(Extension):
  def extendMarkdown(self, md, md_globals):
    md.postprocessors.add('CronetPostprocessor', CronetPostprocessor(md),
                          '_end')


def GenerateJavadoc(args, src_dir, output_dir):
  working_dir = os.path.join(args.input_dir, 'android', 'api')
  overview_file = os.path.abspath(args.overview_file)

  android_sdk_jar = args.android_sdk_jar
  if not android_sdk_jar:
    android_sdk_jar = os.path.join(SDK_DIR, 'platforms', 'android-27',
                                   'android.jar')

  build_utils.DeleteDirectory(output_dir)
  build_utils.MakeDirectory(output_dir)
  classpath = ([android_sdk_jar] + args.support_annotations_jars +
               args.classpath_jars)
  javadoc_cmd = [
      os.path.abspath(JAVADOC_PATH),
      '-d',
      output_dir,
      '-quiet',
      '-overview',
      overview_file,
      '-doclet',
      'com.google.doclava.Doclava',
      '-docletpath',
      '%s:%s' % (os.path.join(DOCLAVA_DIR, 'jsilver.jar'),
                 os.path.join(DOCLAVA_DIR, 'doclava.jar')),
      '-title',
      'Cronet API',
      '-federate',
      'Android',
      'https://developer.android.com/',
      '-federationapi',
      'Android',
      os.path.join(DOCLAVA_DIR, 'current.txt'),
      '-classpath',
      ':'.join(os.path.abspath(p) for p in classpath),
  ]
  for subdir, _, files in os.walk(src_dir):
    for filename in files:
      if filename.endswith(".java"):
        javadoc_cmd += [os.path.join(subdir, filename)]
  try:

    def stderr_filter(stderr):
      return stderr.replace(JAVADOC_WARNING, '')

    build_utils.CheckOutput(javadoc_cmd,
                            cwd=working_dir,
                            stderr_filter=stderr_filter)
  except build_utils.CalledProcessError:
    build_utils.DeleteDirectory(output_dir)
    raise

  # Create an index.html file at the root as this is the accepted format.
  # Do this by copying reference/index.html and adjusting the path.
  with open(os.path.join(output_dir, 'reference', 'index.html'), 'r') as \
      old_index, open(os.path.join(output_dir, 'index.html'), 'w') as new_index:
    for line in old_index:
      new_index.write(
          line.replace('classes.html', os.path.join('reference',
                                                    'classes.html')))


def main(argv):
  parser = argparse.ArgumentParser()
  action_helpers.add_depfile_arg(parser)
  parser.add_argument('--output-dir', help='Directory to put javadoc')
  parser.add_argument('--input-dir', help='Root of cronet source')
  parser.add_argument('--input-src-jar', help='Cronet api source jar')
  parser.add_argument('--overview-file', help='Path of the overview page')
  parser.add_argument('--readme-file', help='Path of the README.md')
  parser.add_argument('--zip-file', help='Path to ZIP archive of javadocs.')
  parser.add_argument('--android-sdk-jar', help='Path to android.jar')
  parser.add_argument('--support-annotations-jars',
                      help='Path to support-annotations-$VERSION.jar',
                      action='append',
                      nargs='*')
  parser.add_argument('--classpath-jars',
                      help='Paths to jars needed by support-annotations-jar.',
                      action='append',
                      nargs='*')
  expanded_argv = build_utils.ExpandFileArgs(argv)
  args, _ = parser.parse_known_args(expanded_argv)

  args.classpath_jars = action_helpers.parse_gn_list(args.classpath_jars)

  args.support_annotations_jars = list(
      itertools.chain(*args.support_annotations_jars))
  # A temporary directory to put the output of cronet api source jar files.
  unzipped_jar_path = tempfile.mkdtemp(dir=args.output_dir)
  if os.path.exists(args.input_src_jar):
    jar_cmd = [
        os.path.relpath(JAR_PATH, unzipped_jar_path), 'xf',
        os.path.abspath(args.input_src_jar)
    ]
    build_utils.CheckOutput(jar_cmd, cwd=unzipped_jar_path)
  else:
    raise Exception('Jar file does not exist: %s' % args.input_src_jar)

  net_docs.ProcessDocs([args.readme_file],
                       args.input_dir,
                       args.output_dir,
                       extensions=[CronetExtension()])

  output_dir = os.path.abspath(os.path.join(args.output_dir, 'javadoc'))
  GenerateJavadoc(args, os.path.abspath(unzipped_jar_path), output_dir)

  if args.zip_file:
    assert args.zip_file.endswith('.zip')
    shutil.make_archive(args.zip_file[:-4], 'zip', output_dir)
  if args.depfile:
    assert args.zip_file
    deps = []
    for root, _, filenames in os.walk(args.input_dir):
      # Ignore .pyc files here, it might be re-generated during build.
      deps.extend(
          os.path.join(root, f) for f in filenames if not f.endswith('.pyc'))
    if args.support_annotations_jars:
      deps.extend(args.support_annotations_jars)
    if args.classpath_jars:
      deps.extend(args.classpath_jars)
    action_helpers.write_depfile(args.depfile, args.zip_file, deps)
  # Clean up temporary output directory.
  build_utils.DeleteDirectory(unzipped_jar_path)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
