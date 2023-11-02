#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Usage:
# Dump the feedv2 stream database from a connected device to a directory on this
# computer.
# > stream_dump.py --device=FA77D0303076 --apk='com.chrome.canary'
# > ls /tmp/feed_dump
#
# Files are output as textproto.
#
# Make any desired modifications, and then upload the dump back to the connected
# device.
# > stream_dump.py --device=FA77D0303076 --apk='com.chrome.canary' --reverse
import argparse
import glob
import os
import plyvel
import protoc_util
import re
import subprocess
import sys

from os.path import join, dirname, realpath

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

ROOT_DIR = realpath(join(dirname(__file__), "../../../../.."))
DUMP_DIR = args.dump_to
DB_PATH = args.db
STREAM_DB_PATH = join(DB_PATH, 'shared_proto_db')
DEVICE_DB_PATH = (
    "/data/data/{}/" + "app_chrome/Default/shared_proto_db").format(args.apk)
STORAGE_PROTO = 'components/feed/core/proto/v2/store.proto'

# From the shared proto db ID, see
# components/leveldb_proto/public/shared_proto_database_client_list.h
KEY_PREFIX = '26_'


def adb_base_args():
    adb_path = join(ROOT_DIR,
                    "third_party/android_sdk/public/platform-tools/adb")
    adb_device = args.device
    if adb_device:
        return [adb_path, "-s", adb_device]
    return [adb_path]


def adb_pull_db():
    subprocess.check_call(adb_base_args() + ["pull", DEVICE_DB_PATH, DB_PATH])


def adb_push_db():
    subprocess.check_call(adb_base_args() +
                          ["push", STREAM_DB_PATH, DEVICE_DB_PATH])


# Extract a binary proto database entry into textproto.
def extract_db_entry(key, data):
    return protoc_util.decode_proto(data, 'feedstore.Record', ROOT_DIR,
                                    STORAGE_PROTO)


# Dump the database to a local directory as textproto files.
def dump():
    os.makedirs(DUMP_DIR, exist_ok=True)
    os.makedirs(DB_PATH, exist_ok=True)
    adb_pull_db()
    db = plyvel.DB(STREAM_DB_PATH, create_if_missing=False)
    with db.iterator() as it:
        for i, (k, v) in enumerate(it):
            k = k.decode('utf-8')
            if not k.startswith(KEY_PREFIX): continue
            key = k[3:]
            with open(join(DUMP_DIR, 'entry{:03d}.key'.format(i)), 'w') as f:
                f.write(key)
            with open(join(DUMP_DIR, 'entry{:03d}.textproto'.format(i)),
                      'w') as f:
                f.write(extract_db_entry(k, v))
    print('Finished dumping to', DUMP_DIR)
    db.close()


# Reverse of dump().
def load():
    db = plyvel.DB(STREAM_DB_PATH, create_if_missing=False)
    # For each textproto file, update its database entry.
    # No attempt is made to delete keys for deleted files.
    for f in os.listdir(DUMP_DIR):
        if f.endswith('.textproto'):
            f_base, _ = os.path.splitext(f)
            with open(join(DUMP_DIR, f_base + '.key'), 'r') as file:
                key = KEY_PREFIX + file.read().strip()
            with open(join(DUMP_DIR, f), 'r') as file:
                value_text_proto = file.read()
            value_encoded = protoc_util.encode_proto(
                value_text_proto, 'feedstore.Record', ROOT_DIR, STORAGE_PROTO)
            db.put(key.encode(), value_encoded)
    db.close()
    adb_push_db()


if not args.reverse:
    dump()
else:
    load()
