# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools


# Use functools.lru_cache in Python 3 and up.
def memoize(cache):
    """Minimal memoization decorator"""
    def decorator(func):
        def wrapper(*args):
            try:
                return cache[args]
            except KeyError:
                pass  # Not found

            result = func(*args)
            cache[args] = result
            return result

        return functools.update_wrapper(wrapper, func)

    return decorator


class EnumValue(object):
    """Minimal enum value wrapper to look like Python3's Enum class."""
    def __init__(self, value):
        self.value = value