# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import logging
import os
import platform
import shutil
import subprocess
import threading

from chrome.test.variations.test_utils.defines import SRC_DIR
import packaging
import packaging.version

GSUTIL_PATH = os.path.join(
    SRC_DIR, 'third_party', 'catapult', 'third_party', 'gsutil', 'gsutil')


@functools.lru_cache
def find_gsutil_cmd() -> str:
  """Returns the gsutil command to use to access GCS buckets.

  'gsutil.py' (from depot_tools) is preferred as it authenticates via
  luci-auth. The 'gsutil' bundled in third_party/catapult is only used as a
  last resort: plain 'gsutil' is deprecated and falls back to anonymous
  access, which cannot read the internal buckets.
  """
  if gsutil := (shutil.which('gsutil.py') or shutil.which('gsutil')):
    return gsutil
  if os.path.exists(GSUTIL_PATH):
    return GSUTIL_PATH
  raise RuntimeError("Please specify script path for gsutil or run "
                     "'sudo apt install google-cloud-sdk' and try again.")


def check_chrome_version(downloaded_chrome: str) -> packaging.version.Version:
  host = get_hosted_platform()
  if host == 'win':
    cmd = ('powershell -command "&{(Get-Item'
            '\''+ downloaded_chrome + '\').VersionInfo.ProductVersion}"')
    version = subprocess.run(cmd, check=True,
                          capture_output=True).stdout.decode('utf-8')
  else:
    cmd = [downloaded_chrome, '--version']
    version = subprocess.run(cmd, check=True,
                          capture_output=True).stdout.decode('utf-8')
    # Only return the version number portion
    version = version.strip().split(' ')[-1]
  return packaging.version.parse(version)

@functools.lru_cache
def get_hosted_platform() -> str:
  """Returns the host platform.

  Returns: One of 'linux', 'win' and 'mac'.
  """
  host = platform.uname().system.lower()
  if host in ('win32', 'cygwin', 'windows'):
    return 'win'
  if host.startswith('linux'):
    return 'linux'
  if host == 'darwin':
    return 'mac'

  raise RuntimeError('Unknown or unsupported host platform (%s).' %
                     platform.uname())

class TimeoutError(Exception):
    """Raised when a function call exceeds the timeout."""
    pass

def timeout(seconds: float):
    """
    A decorator that stops a function call after a specified number of seconds.

    Args:
        seconds: The timeout duration in seconds.

    Returns:
        The decorated function.
    """
    def decorator(func):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            result_container = {'result': None, 'exception': None}

            def target():
                try:
                    result_container['result'] = func(*args, **kwargs)
                except Exception as e:
                    result_container['exception'] = e

            thread = threading.Thread(target=target)
            # Make the thread a daemon. In Python we have no way to
            # kill a running thread, therefore we have to allow it to continue
            # running in the background.
            thread.daemon = True
            thread.start()
            thread.join(seconds)

            if thread.is_alive():
                # We can't directly kill a thread, but by making it a daemon
                # it will be terminated when the main program exits.
                # Note that it will not be terminated gracefully therefore
                # resources may not be released properly.
                raise TimeoutError(
                  f"Function '{func.__name__}' timed out after {seconds} "
                  "seconds."
                )
            elif result_container['exception']:
                # Function raised an exception, re-raising it
                raise result_container['exception']
            else:
                # Function finished within timeout, returning the result
                return result_container['result']
        return wrapper
    return decorator

def retry(count: int):
    """
    A decorator that reties execution of the wrapped function.

    Args:
        count: How many times the function execution should be retried.

    Returns:
        The decorated function.
    """
    def decorator(func):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
           trial = 1
           while True:
              try:
                  return func(*args, **kwargs)
              except Exception as e:
                  if trial < count:
                      logging.warning(
                         f"#{trial} execution of {func.__name__} failed " +
                         f"with error {e!r}, " +
                         f"remaining {count - trial} / {count} reties.")
                      trial += 1
                  else:
                      logging.warning(
                         f"#{trial} execution of {func.__name__} failed."
                      )
                      raise
        return wrapper
    return decorator
