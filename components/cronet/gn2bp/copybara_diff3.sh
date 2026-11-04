#!/bin/sh
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Wrapper passed to Copybara via --diff-bin so that diff3 runs with -E
# (--show-overlap). Without -E, GNU diff3 -m defaults to -A (--show-all), which
# produces spurious merge conflicts on hunks that changed identically in both
# origin and destination whenever another part of the same file also differs
# (b/392575734).
exec diff3 -E "$@"
