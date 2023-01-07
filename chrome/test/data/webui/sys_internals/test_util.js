// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


export function assertCloseTo(value, equ, delta, optMessage) {
  chai.assert.closeTo(value, equ, delta, optMessage);
}

export const MEMORY_UNITS = {
  B: 1,
  KB: Math.pow(1024, 1),
  MB: Math.pow(1024, 2),
  GB: Math.pow(1024, 3),
  TB: Math.pow(1024, 4),
  PB: Math.pow(1024, 5),
};

export function getTestData(cpuData) {
  const GB = MEMORY_UNITS.GB;
  const TB = MEMORY_UNITS.TB;
  return {
    const : {counterMax: 2147483647},
    cpus: cpuData,
    memory: {
      available: 4 * TB,
      pswpin: 1234,
      pswpout: 1234,
      swapFree: 4 * TB,
      swapTotal: 6 * TB,
      total: 8 * TB,
    },
    zram: {
      comprDataSize: 100 * GB,
      memUsedTotal: 300 * GB,
      numReads: 1234,
      numWrites: 1234,
      origDataSize: 200 * GB,
    },
  };
}
