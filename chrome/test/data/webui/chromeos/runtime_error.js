// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Function to be called by
 * WebUIBrowserExpectFailTest.TestRuntimeErrorFailsFast.
 */
function TestRuntimeErrorFailsFast() {}

// Call a method that doesn't exist to test that runtime errors fail outside
// scope of calling the test function.
TestRuntimeErrorFailsFast.badMethod();
