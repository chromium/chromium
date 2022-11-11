// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

async function mockTcp() {
  return typeof TCPSocket !== 'undefined';
}

async function mockUdp() {
  return typeof UDPSocket !== 'undefined';
}
