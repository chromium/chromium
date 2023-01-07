// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The qualification app is a do-nothing application "installer" that is
// embedded in an update CRX. In the future, it could be expanded to do
// installer-like things to provide a more comprehensive test of the updater.
// For example, it could verify that the application installer is being called
// with the proper environment variables or arguments. It must not mutate the
// state of the system, except possibly in a side-by-side manner.
int main() {}
