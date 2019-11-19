#!/usr/bin/python3
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Usage:
# Dump the feed content database from a connected device to a directory on this
# computer.
# > content_dump.py --device=FA77D0303076 --apk='com.chrome.canary'
# > ls /tmp/feed_dump
#
# Files are output as textproto.
#
# Make any desired modifications, and then upload the dump back to the connected
# device.
# > content_dump.py --device=FA77D0303076 --apk='com.chrome.canary' --reverse
import os
import re
import sys
import argparse
import subprocess
import glob
from os.path import join, dirname, realpath

import plyvel

# A dynamic import for encoding and decoding of escaped textproto strings.
_prototext_mod = None


# Import text proto escape/unescape functions from third_party/protobuf.
def prototext():
  global _prototext_mod
  import importlib.util
  if _prototext_mod:
    return _prototext_mod
  source_path = join(
      dirname(__file__),
      "../../../third_party/protobuf/python/google/protobuf/text_encoding.py")
  spec = importlib.util.spec_from_file_location("protobuf.textutil",
                                                source_path)
  _prototext_mod = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(_prototext_mod)
  return _prototext_mod


parser = argparse.ArgumentParser()
parser.add_argument("--db", help="Path to db", default='/tmp/feed_dump/db')
parser.add_argument(
    "--dump_to", help="Dump output directory", default='/tmp/feed_dump')
parser.add_argument(
    "--reverse", help="Write dump back to database", action='store_true')
parser.add_argument("--device", help="adb device to use")
parser.add_argument(
    "--apk", help="APK to dump from/to", default='com.chrome.canary')

args = parser.parse_args()

ROOT_DIR = realpath(join(dirname(__file__), "../../.."))
DUMP_DIR = args.dump_to
DB_PATH = args.db
CONTENT_DB_PATH = join(DB_PATH, 'content')
DEVICE_DB_PATH = "/data/data/{}/app_chrome/Default/feed".format(args.apk)
_protoc_path = None


# Returns the path to the proto compiler, protoc.
def protoc_path():
  global _protoc_path
  if not _protoc_path:
    protoc_list = list(glob.glob(join(ROOT_DIR, "out") + "/*/protoc")) + list(
        glob.glob(join(ROOT_DIR, "out") + "/*/*/protoc"))
    if not len(protoc_list):
      print("Can't find a suitable build output directory",
            "(it should have protoc)")
      sys.exit(1)
    _protoc_path = protoc_list[0]
  return _protoc_path


def adb_base_args():
  adb_path = join(ROOT_DIR, "third_party/android_sdk/public/platform-tools/adb")
  adb_device = args.device
  if adb_device:
    return [adb_path, "-s", adb_device]
  return [adb_path]


def adb_pull_db():
  subprocess.check_call(
      adb_base_args() +
      ["pull", join(DEVICE_DB_PATH, 'content'), DB_PATH])


def adb_push_db():
  subprocess.check_call(adb_base_args() +
                        ["push", CONTENT_DB_PATH, DEVICE_DB_PATH])


def get_feed_protos():
  result = [join(ROOT_DIR, 'components/feed/core/proto/content_storage.proto')]
  for root, _, files in os.walk(join(ROOT_DIR, "third_party/feed")):
    result += [join(root, f) for f in files if f.endswith('.proto')]

  return result


protoc_common_args = [
    '-I' + join(ROOT_DIR, 'third_party/feed/src'), '-I' + join(ROOT_DIR)
] + get_feed_protos()


def run_command(args, input):
  proc = subprocess.run(
      args,
      input=input,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      check=True)
  return proc.stdout


# Decode a binary proto into textproto format.
def decode_proto(data, message_name):
  return run_command(
      [protoc_path(), '--decode=' + message_name] + protoc_common_args,
      data).decode('utf-8')


# Encode a textproto into binary proto format.
def encode_proto(text, message_name):
  return run_command(
      [protoc_path(), '--encode=' + message_name] + protoc_common_args,
      text.encode())


# Ignore DB entries with the 'sp::' prefix, as they are not yet supported.
def is_key_supported(key):
  return not key.startswith('sp::')


# Return the proto message stored under the given db key.
def proto_message_from_db_key(key):
  if key.startswith('ss::'):
    return 'search.now.feed.client.StreamSharedState'
  if key.startswith('FEATURE::') or key.startswith('FSM::'):
    return 'search.now.feed.client.StreamPayload'
  print("Unknown Key kind", key)
  sys.exit(1)


# Extract a binary proto database entry into textproto.
def extract_db_entry(key, data):
  # DB entries are feed.ContentStorageProto messages. First extract
  # the content_data contained within.
  text_proto = decode_proto(data, 'feed.ContentStorageProto')
  m = re.search(r"content_data: \"((?:\\\"|[^\"])*)\"", text_proto)
  raw_data = prototext().CUnescape(m.group(1))

  # Next, convert raw_data into a textproto. The DB key informs which message
  # is stored.
  result = decode_proto(raw_data, proto_message_from_db_key(key))
  return result


# Dump the content database to a local directory as textproto files.
def dump():
  os.makedirs(DUMP_DIR, exist_ok=True)
  os.makedirs(DB_PATH, exist_ok=True)
  adb_pull_db()
  db = plyvel.DB(CONTENT_DB_PATH, create_if_missing=False)
  with db.iterator() as it:
    for i, (k, v) in enumerate(it):
      k = k.decode('utf-8')
      if not is_key_supported(k):
        continue
      with open(join(DUMP_DIR, 'entry{:03d}.key'.format(i)), 'w') as f:
        f.write(k)
      with open(join(DUMP_DIR, 'entry{:03d}.textproto'.format(i)), 'w') as f:
        f.write(extract_db_entry(k, v))
  print('Finished dumping to', DUMP_DIR)
  db.close()


# Reverse of dump().
def load():
  db = plyvel.DB(CONTENT_DB_PATH, create_if_missing=False)
  # For each textproto file, update its database entry.
  # No attempt is made to delete keys for deleted files.
  for f in os.listdir(DUMP_DIR):
    if f.endswith('.textproto'):
      f_base, _ = os.path.splitext(f)
      with open(join(DUMP_DIR, f_base + '.key'), 'r') as file:
        key = file.read().strip()
      with open(join(DUMP_DIR, f), 'r') as file:
        value_text_proto = file.read()
      value_encoded = encode_proto(value_text_proto,
                                   proto_message_from_db_key(key))
      # Create binary feed.ContentStorageProto by encoding its textproto.
      content_storage_text = 'key: "{}"\ncontent_data: "{}"'.format(
          prototext().CEscape(key, False),
          prototext().CEscape(value_encoded, False))

      store_encoded = encode_proto(content_storage_text,
                                   'feed.ContentStorageProto')
      db.put(key.encode(), store_encoded)
  db.close()
  adb_push_db()


if not args.reverse:
  dump()
else:
  load()
