#!/usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""api_static_checks.py - Enforce Cronet API requirements."""

import argparse
import os
import re
import shutil
import sys
import tempfile

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

sys.path.insert(0, os.path.join(REPOSITORY_ROOT, 'build/android/gyp'))
from util import build_utils  # pylint: disable=wrong-import-position

sys.path.insert(0, os.path.join(REPOSITORY_ROOT, 'components'))
from cronet.tools import update_api  # pylint: disable=wrong-import-position


# These regular expressions catch the beginning of lines that declare classes
# and methods.  The first group returned by a match is the class or method name.
from cronet.tools.update_api import CLASS_RE  # pylint: disable=wrong-import-position
METHOD_RE = re.compile(r'.* ([^ ]*)\(.*\);')

# Allowed exceptions.  Adding anything to this list is dangerous and should be
# avoided if possible.  For now these exceptions are for APIs that existed in
# the first version of Cronet and will be supported forever.
# TODO(pauljensen): Remove these.
ALLOWED_EXCEPTIONS = [
'org.chromium.net.impl.CronetEngineBuilderImpl/build ->'
    ' org/chromium/net/ExperimentalCronetEngine/getVersionString:'
    '()Ljava/lang/String;',
'org.chromium.net.urlconnection.CronetFixedModeOutputStream$UploadDataProviderI'
    'mpl/read -> org/chromium/net/UploadDataSink/onReadSucceeded:(Z)V',
'org.chromium.net.urlconnection.CronetFixedModeOutputStream$UploadDataProviderI'
    'mpl/rewind -> org/chromium/net/UploadDataSink/onRewindError:'
    '(Ljava/lang/Exception;)V',
'org.chromium.net.urlconnection.CronetHttpURLConnection/disconnect ->'
    ' org/chromium/net/UrlRequest/cancel:()V',
'org.chromium.net.urlconnection.CronetHttpURLConnection/disconnect ->'
    ' org/chromium/net/UrlResponseInfo/getHttpStatusText:()Ljava/lang/String;',
'org.chromium.net.urlconnection.CronetHttpURLConnection/disconnect ->'
    ' org/chromium/net/UrlResponseInfo/getHttpStatusCode:()I',
'org.chromium.net.urlconnection.CronetHttpURLConnection/getHeaderField ->'
    ' org/chromium/net/UrlResponseInfo/getHttpStatusCode:()I',
'org.chromium.net.urlconnection.CronetHttpURLConnection/getErrorStream ->'
    ' org/chromium/net/UrlResponseInfo/getHttpStatusCode:()I',
'org.chromium.net.urlconnection.CronetHttpURLConnection/setConnectTimeout ->'
    ' org/chromium/net/UrlRequest/read:(Ljava/nio/ByteBuffer;)V',
'org.chromium.net.urlconnection.CronetHttpURLConnection$CronetUrlRequestCallbac'
    'k/onRedirectReceived -> org/chromium/net/UrlRequest/followRedirect:()V',
'org.chromium.net.urlconnection.CronetHttpURLConnection$CronetUrlRequestCallbac'
    'k/onRedirectReceived -> org/chromium/net/UrlRequest/cancel:()V',
'org.chromium.net.urlconnection.CronetChunkedOutputStream$UploadDataProviderImp'
    'l/read -> org/chromium/net/UploadDataSink/onReadSucceeded:(Z)V',
'org.chromium.net.urlconnection.CronetChunkedOutputStream$UploadDataProviderImp'
    'l/rewind -> org/chromium/net/UploadDataSink/onRewindError:'
    '(Ljava/lang/Exception;)V',
'org.chromium.net.urlconnection.CronetBufferedOutputStream$UploadDataProviderIm'
    'pl/read -> org/chromium/net/UploadDataSink/onReadSucceeded:(Z)V',
'org.chromium.net.urlconnection.CronetBufferedOutputStream$UploadDataProviderIm'
    'pl/rewind -> org/chromium/net/UploadDataSink/onRewindSucceeded:()V',
'org.chromium.net.urlconnection.CronetHttpURLStreamHandler/org.chromium.net.url'
    'connection.CronetHttpURLStreamHandler -> org/chromium/net/ExperimentalCron'
    'etEngine/openConnection:(Ljava/net/URL;)Ljava/net/URLConnection;',
'org.chromium.net.urlconnection.CronetHttpURLStreamHandler/org.chromium.net.url'
    'connection.CronetHttpURLStreamHandler -> org/chromium/net/ExperimentalCron'
    'etEngine/openConnection:(Ljava/net/URL;Ljava/net/Proxy;)Ljava/net/URLConne'
    'ction;',
# getMessage() is an java.lang.Exception member, and so cannot be removed.
'org.chromium.net.impl.NetworkExceptionImpl/getMessage -> '
    'org/chromium/net/NetworkException/getMessage:()Ljava/lang/String;',
]


def find_api_calls(dump, api_classes, bad_calls):
  # Given a dump of an implementation class, find calls through API classes.
  # |dump| is the output of "javap -c" on the implementation class files.
  # |api_classes| is the list of classes comprising the API.
  # |bad_calls| is the list of calls through API classes.  This list is built up
  #             by this function.

  for line in dump:
    if CLASS_RE.match(line):
      caller_class = CLASS_RE.match(line).group(1)
    if METHOD_RE.match(line):
      caller_method = METHOD_RE.match(line).group(1)
    if line[8:16] == ': invoke':
      callee = line.split(' // ')[1].split('Method ')[1].split('\n')[0]
      callee_class = callee.split('.')[0]
      assert callee_class
      if callee_class in api_classes:
        callee_method = callee.split('.')[1]
        assert callee_method
        # Ignore constructor calls for now as every implementation class
        # that extends an API class will call them.
        # TODO(pauljensen): Look into enforcing restricting constructor calls.
        # https://crbug.com/674975
        if callee_method.startswith('"<init>"'):
          continue
        # Ignore VersionSafe calls
        if 'VersionSafeCallbacks' in caller_class:
          continue
        bad_call = '%s/%s -> %s/%s' % (caller_class, caller_method,
                                       callee_class, callee_method)
        if bad_call in ALLOWED_EXCEPTIONS:
          continue
        bad_calls += [bad_call]


def check_api_calls(opts):
  # Returns True if no calls through API classes in implementation.

  temp_dir = tempfile.mkdtemp()

  # Extract API class files from jar
  jar_cmd = ['jar', 'xf', os.path.abspath(opts.api_jar)]
  build_utils.CheckOutput(jar_cmd, cwd=temp_dir)
  shutil.rmtree(os.path.join(temp_dir, 'META-INF'), ignore_errors=True)

  # Collect names of API classes
  api_classes = []
  for dirpath, _, filenames in os.walk(temp_dir):
    if not filenames:
      continue
    package = os.path.relpath(dirpath, temp_dir)
    for filename in filenames:
      if filename.endswith('.class'):
        classname = filename[:-len('.class')]
        api_classes += [os.path.normpath(os.path.join(package, classname))]

  shutil.rmtree(temp_dir)
  temp_dir = tempfile.mkdtemp()

  # Extract impl class files from jars
  for impl_jar in opts.impl_jar:
    jar_cmd = ['jar', 'xf', os.path.abspath(impl_jar)]
    build_utils.CheckOutput(jar_cmd, cwd=temp_dir)
  shutil.rmtree(os.path.join(temp_dir, 'META-INF'), ignore_errors=True)

  # Process classes
  bad_api_calls = []
  for dirpath, _, filenames in os.walk(temp_dir):
    if not filenames:
      continue
    # Dump classes
    dump_file = os.path.join(temp_dir, 'dump.txt')
    if os.system('javap -c %s > %s' % (
        ' '.join(os.path.join(dirpath, f) for f in filenames).replace(
            '$', '\\$'),
        dump_file)):
      print 'ERROR: javap failed on ' + ' '.join(filenames)
      return False
    # Process class dump
    with open(dump_file, 'r') as dump:
      find_api_calls(dump, api_classes, bad_api_calls)

  shutil.rmtree(temp_dir)

  if bad_api_calls:
    print 'ERROR: Found the following calls from implementation classes through'
    print '       API classes.  These could fail if older API is used that'
    print '       does not contain newer methods.  Please call through a'
    print '       wrapper class from VersionSafeCallbacks.'
    print '\n'.join(bad_api_calls)
  return not bad_api_calls


def check_api_version(opts):
  if update_api.check_up_to_date(opts.api_jar):
    return True
  print 'ERROR: API file out of date.  Please run this command:'
  print '       components/cronet/tools/update_api.py --api_jar %s' % (
      os.path.abspath(opts.api_jar))
  return False


def main(args):
  parser = argparse.ArgumentParser(
      description='Enforce Cronet API requirements.')
  parser.add_argument('--api_jar',
                      help='Path to API jar (i.e. cronet_api.jar)',
                      required=True,
                      metavar='path/to/cronet_api.jar')
  parser.add_argument('--impl_jar',
                      help='Path to implementation jar '
                          '(i.e. cronet_impl_native_java.jar)',
                      required=True,
                      metavar='path/to/cronet_impl_native_java.jar',
                      action='append')
  parser.add_argument('--stamp', help='Path to touch on success.')
  opts = parser.parse_args(args)

  ret = True
  ret = check_api_calls(opts) and ret
  ret = check_api_version(opts) and ret
  if ret and opts.stamp:
    build_utils.Touch(opts.stamp)
  return ret


if __name__ == '__main__':
  sys.exit(0 if main(sys.argv[1:]) else -1)
