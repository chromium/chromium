# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import enum


class LicenseType(enum.Enum):
    # The higher the value, the higher the restrictions.
    UNKNOWN = 0
    UNENCUMBERED = 1
    PERMISSIVE = 2
    NOTICE = 3
    RECIPROCAL = 4
    RESTRICTED = 5
    BY_EXCEPTION_ONLY = 6
