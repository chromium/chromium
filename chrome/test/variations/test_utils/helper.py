# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import platform
import subprocess
import threading

import packaging
import packaging.version


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