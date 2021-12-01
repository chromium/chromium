#!/usr/bin/env python/2/3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import secrets


def main():
    print(secrets.token_urlsafe(32))


if __name__ == '__main__':
    main()
