// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_SETUP_UTIL_UNITTEST_H_
#define CHROME_INSTALLER_SETUP_SETUP_UTIL_UNITTEST_H_

// A command line switch that causes the test harness to exit with the result of
// DoProcessPriorityAdjustment rather than executing all tests.
extern const char kAdjustProcessPriority[];

// Process exit codes when the test harness is run with the
// kAdjustProcessPriority switch.
enum PriorityClassChangeResult {
  PCCR_UNKNOWN,
  PCCR_UNCHANGED,
  PCCR_CHANGED,
};

// Calls AdjustProcessPriority() and returns PCCR_CHANGED or PCCR_UNCHANGED
// based on its true or false result (respectively).
PriorityClassChangeResult DoProcessPriorityAdjustment();

#endif  // CHROME_INSTALLER_SETUP_SETUP_UTIL_UNITTEST_H_
