# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Per-OS knowledge about the sender DUT.

Each supported --sender_os maps to one Sender subclass. make_sender() is the
only place that looks at the OS name; everything else calls Sender methods.
"""

import abc
import glob
import json
import logging
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.request

import perf_config
import remote_transport
from remote_transport import INFRA_FAILURE_NOTE, RemoteDeviceError

_STATUS_URL = (f'http://{perf_config.LOCAL_HOST_IP}:'
               f'{perf_config.CHROMEDRIVER_PORT}/status')
_CHROMEDRIVER_FLAGS = (f'--port={perf_config.CHROMEDRIVER_PORT} '
                       '--disable-ipv6 --allowed-origins="*" --allowed-ips= '
                       '--verbose --log-path=/tmp/chromedriver_verbose.log '
                       '--enable-chrome-logs')
_CHROMEDRIVER_CHECK_ATTEMPTS = 15
_CHROMEDRIVER_READY_ATTEMPTS = 30

WIN_REMOTE_TMP_DIR = 'C:/cft_temp'
WIN_SYSTEM32_TAR = 'C:/Windows/System32/tar.exe'


def download_cft_urls(platform_name, version=None):
    """Finds Chrome for Testing download URLs.

    Args:
        platform_name: CfT platform, e.g. 'mac-arm64' or 'win64'.
        version: Exact version or milestone prefix. None means the newest.

    Returns:
        (version, chrome_url, chromedriver_url)
    """
    logging.info("Downloading Chrome for Testing JSON data...")
    with urllib.request.urlopen(perf_config.CFT_JSON_URL) as url:
        data = json.loads(url.read().decode())

    for v in reversed(data['versions']):
        # Match exact version OR the beginning of a version.
        if not version or v['version'] == version or v['version'].startswith(
                f"{version}."):
            chrome_url = None
            driver_url = None
            for download in v['downloads']['chrome']:
                if download['platform'] == platform_name:
                    chrome_url = download['url']
            for download in v['downloads']['chromedriver']:
                if download['platform'] == platform_name:
                    driver_url = download['url']
            if chrome_url and driver_url:
                logging.info("Found URLs for version %s on platform %s",
                             v['version'], platform_name)
                return v['version'], chrome_url, driver_url

    raise RuntimeError(
        f"Could not find downloads for version {version} on {platform_name}")


def _zip_name_and_dir(url):
    """Returns ('foo.zip', 'foo') for a URL ending in foo.zip."""
    zip_name = url.split('/')[-1]
    return zip_name, zip_name.replace('.zip', '')


class Sender(abc.ABC):
    """A machine that runs Chrome for the perf tests.

    Subclasses provide the OS-specific commands. All commands go through
    `self.transport`, so the same code works for remote and local senders.
    """

    # Value of --sender_os.
    OS_NAME = ''
    # Human-readable name for log messages.
    DISPLAY_NAME = ''
    # Where Chrome for Testing is downloaded and unzipped.
    TMP_DIR = '/tmp'
    # Maps detected arch to Chrome for Testing platform name.
    CFT_PLATFORMS = {}
    TERMINATE_CHROMEDRIVER_CMD = ''
    CHROMEDRIVER_CHECK_CMD = ''
    STATUS_CMD = ''
    GLANCES_PYTHON = 'python3'
    GLANCES_KILL_CMD = 'pkill -f glances'

    def __init__(self, transport):
        self.transport = transport

    @abc.abstractmethod
    def _detect_arch(self, probes):
        """Returns the arch ('x64', 'arm64', ...), or None/'' on failure."""

    @abc.abstractmethod
    def _detect_os_version(self, probes):
        """Returns the OS version string, or None/'' on failure."""

    @abc.abstractmethod
    def _install(self, chrome_url, driver_url):
        """Installs Chrome on a remote sender and returns the app path."""

    @abc.abstractmethod
    def _console_log_command(self):
        """Returns the command that prints chromedriver's console log."""

    @abc.abstractmethod
    def _downloads_cleanup_command(self):
        """Returns the command that deletes downloaded zips."""

    @abc.abstractmethod
    def _binaries_cleanup_command(self):
        """Returns the command that deletes installed Chrome/chromedriver."""

    def run(self, command):
        """Runs `command` on the sender and returns the CompletedProcess."""
        return self.transport.run(command)

    def copy_from(self,
                  remote_path,
                  local_path,
                  timeout=remote_transport.COPY_TIMEOUT_SECS):
        """Copies `remote_path` on the sender to `local_path` here."""
        self.transport.copy_from(remote_path, local_path, timeout=timeout)

    def verify_connectivity(self):
        """Raises RemoteDeviceError if the sender cannot run commands."""
        self.transport.verify_connectivity()

    # --- Platform detection ---

    def detect_platform(self):
        """Detects the sender's CPU arch and OS version.

        Returns:
            {'arch': str, 'os_version': str}

        Raises:
            RemoteDeviceError: A probe returned no usable value.
        """
        self.verify_connectivity()
        if self.transport.is_local:
            arch = platform.machine()
            return {
                'arch': 'x64' if arch == 'x86_64' else arch,
                'os_version': platform.release()
            }

        arch_probes = []
        version_probes = []
        info = {
            'arch': self._detect_arch(arch_probes),
            'os_version': self._detect_os_version(version_probes),
        }
        self._raise_if_missing(info, {
            'arch': arch_probes,
            'os_version': version_probes
        })
        return info

    def _probe(self, command, probes):
        """Runs `command`, records it in `probes`, returns stripped stdout."""
        result = self.run(command)
        probes.append((command, result))
        return result.stdout.strip()

    def _raise_if_missing(self, info, probes_by_field):
        missing = []
        details = []
        for field, probes in probes_by_field.items():
            if info[field]:
                continue
            missing.append(field)
            for command, result in probes:
                details.append(
                    f'  {field} probe {command!r}: rc={result.returncode}, '
                    f'stdout={result.stdout!r}, stderr={result.stderr!r}')
        if not missing:
            return
        raise RemoteDeviceError(
            f"Could not detect {' and '.join(missing)} of sender "
            f"'{self.transport.host}' (sender_os={self.OS_NAME}). "
            f"{INFRA_FAILURE_NOTE}\n" + '\n'.join(details))

    # --- chromedriver lifecycle ---

    def terminate_chromedriver(self):
        """Kills chromedriver and Chrome, and waits for them to exit."""
        self.verify_connectivity()
        logging.info("Attempting to terminate old chromedriver processes...")
        result = self.run(self.TERMINATE_CHROMEDRIVER_CMD)
        self.transport.raise_if_disconnected(result,
                                             'terminating chromedriver')

        for _ in range(_CHROMEDRIVER_CHECK_ATTEMPTS):
            result = self.run(self.CHROMEDRIVER_CHECK_CMD)
            # grep/pgrep exit 1 when nothing matches, which is the expected
            # result here. Only 255 means ssh itself failed.
            self.transport.raise_if_disconnected(result,
                                                 'checking for chromedriver')
            if not result.stdout.strip():
                logging.info("Old chromedriver processes confirmed gone.")
                return
            logging.info(
                "Old chromedriver processes still present, waiting...")
            time.sleep(1)
        raise RuntimeError(
            "Chromedriver processes lingered after kill attempts.")

    def wait_for_chromedriver(self):
        """Polls chromedriver's /status until it answers 200."""
        logging.info("Starting Chromedriver status check...")
        for i in range(_CHROMEDRIVER_READY_ATTEMPTS):
            try:
                result = self.run(self.STATUS_CMD)
                stdout = result.stdout.strip()
                if result.returncode == 0 and stdout == '200':
                    logging.info("Chromedriver is ready.")
                    return
                logging.warning(
                    "Attempt %d failed. Chromedriver not ready. "
                    "Return code: %d, stdout: '%s', stderr: '%s'", i + 1,
                    result.returncode, stdout, result.stderr.strip())
            except subprocess.TimeoutExpired:
                logging.warning("Status check timed out. Retrying...")
            except Exception as e:  # pylint: disable=broad-exception-caught
                logging.warning(
                    "A script-level error occurred: %s. Retrying...", e)
            time.sleep(2)

        logging.error("Chromedriver failed to start.")
        self.dump_console_logs()
        raise RuntimeError(
            "Chromedriver still not ready after multiple attempts.")

    def dump_console_logs(self):
        """Logs chromedriver's console output from the sender."""
        logging.error("Dumping remote console logs:")
        result = self.run(self._console_log_command())
        if result.stdout.strip() or result.stderr.strip():
            logging.error("REMOTE CONSOLE LOG:\nSTDOUT: %s\nSTDERR: %s",
                          result.stdout, result.stderr)

    def start_tunnel(self):
        """Forwards chromedriver to this host and the test server back.

        Returns:
            The tunnel process, or None for a local sender.
        """
        return self.transport.open_tunnel(perf_config.CHROMEDRIVER_PORT,
                                          perf_config.SERVER_PORT)

    # --- Chrome install ---

    def install_chrome(self, chrome_version):
        """Installs Chrome for Testing and starts a matching chromedriver.

        Args:
            chrome_version: Exact version or milestone. None means newest.

        Returns:
            (path to the installed Chrome app, actual Chrome version)
        """
        self.verify_connectivity()
        info = self.detect_platform()
        logging.info("Detected remote info: %s", info)
        arch = info['arch']
        if arch not in self.CFT_PLATFORMS:
            raise NotImplementedError(
                f"Unsupported OS/Arch: {self.OS_NAME}/{arch}")

        actual_version, chrome_url, driver_url = download_cft_urls(
            self.CFT_PLATFORMS[arch], chrome_version)
        logging.info("Downloading Chrome and Chromedriver.")
        if self.transport.is_local:
            return self._install_locally(chrome_url,
                                         driver_url), actual_version

        app_path = self._install(chrome_url, driver_url)
        logging.info("Finished chromedriver setup attempt.")
        return app_path, actual_version

    def _install_locally(self, chrome_url, driver_url):
        """Installs Chrome and starts chromedriver on this machine."""
        tmp_dir = '/tmp'
        chrome_zip, chrome_dir = _zip_name_and_dir(chrome_url)
        driver_zip, driver_dir = _zip_name_and_dir(driver_url)
        if sys.platform == 'linux':
            app_path = f"{tmp_dir}/{chrome_dir}/chrome"
        else:
            app_path = f"{tmp_dir}/{chrome_dir}/Google Chrome for Testing.app"
        driver_path = f"{tmp_dir}/{driver_dir}/chromedriver"

        if os.path.exists(app_path) and os.path.exists(driver_path):
            logging.info("Chrome and Chromedriver already installed locally. "
                         "Skipping download/extract.")
        else:
            subprocess.run(
                f"curl -L {chrome_url} -o {tmp_dir}/{chrome_zip} && "
                f"curl -L {driver_url} -o {tmp_dir}/{driver_zip} && "
                f"unzip -o {tmp_dir}/{chrome_zip} -d {tmp_dir} && "
                f"unzip -o {tmp_dir}/{driver_zip} -d {tmp_dir}",
                shell=True,
                check=True,
                timeout=120)
        subprocess.run(f'chmod +x {driver_path}', shell=True, check=True)
        self.transport.spawn(f'nohup {driver_path} {_CHROMEDRIVER_FLAGS} '
                             f'> /tmp/chromedriver_console.log 2>&1 &')
        logging.info("Finished local chromedriver setup.")
        return app_path

    # --- Cleanup ---

    def cleanup_downloads(self):
        """Removes downloaded zips from the sender. Never raises."""
        if self.transport.known_unreachable:
            logging.warning('Skipping remote cleanup: sender unreachable.')
            return
        self._run_cleanup(self._downloads_cleanup_command(),
                          'tmp files on remote machine')

    def cleanup_binaries(self):
        """Removes installed Chrome/chromedriver from the sender.

        Runs from `finally` blocks, so it never raises.
        """
        logging.info("Cleaning up Chrome/Chromedriver directories...")
        if self.transport.known_unreachable:
            logging.warning('Skipping remote cleanup: sender unreachable.')
            return

        try:
            self.terminate_chromedriver()
        except Exception as e:  # pylint: disable=broad-exception-caught
            logging.warning("Error terminating processes in cleanup: %s", e)

        if self.transport.is_local:
            _remove_local_chrome_dirs()
            return

        # terminate_chromedriver may have just found the sender unreachable.
        if self.transport.known_unreachable:
            logging.warning('Skipping remote cleanup: sender unreachable.')
            return
        self._run_cleanup(self._binaries_cleanup_command(),
                          'remote Chrome/Chromedriver directories')

    def _run_cleanup(self, command, what):
        try:
            result = self.run(command)
        except Exception as e:  # pylint: disable=broad-exception-caught
            logging.warning('Failed to clean up %s: %s', what, e)
            return
        if result.returncode == 0:
            logging.info('Cleaned up %s.', what)
        else:
            logging.warning('Failed to clean up %s (rc=%d): %s', what,
                            result.returncode, (result.stderr or '').strip())

    # --- Resource monitoring ---

    def start_monitoring(self, csv_remote_path):
        """Starts glances in the background, writing CSV to the sender."""
        glances_cmd = (f"{self.GLANCES_PYTHON} -m glances -t 1 --export csv "
                       f"--export-csv-file {csv_remote_path} --quiet")
        logging.info("Starting Glances monitoring on sender...")
        return self.transport.spawn(glances_cmd)

    def stop_monitoring(self, monitor_proc, csv_remote_path, csv_local_path):
        """Stops monitoring and copies its output to `csv_local_path`."""
        logging.info("Stopping Glances/Power monitoring...")
        if monitor_proc:
            try:
                monitor_proc.terminate()
                monitor_proc.wait(timeout=5)
            except Exception:  # pylint: disable=broad-exception-caught
                pass

        # Also kill it on the sender in case terminating ssh did not.
        self.run(self.GLANCES_KILL_CMD)
        self.copy_from(csv_remote_path, csv_local_path)
        self.run(f"rm -f {csv_remote_path}")


def _remove_local_chrome_dirs():
    tmp_dir = tempfile.gettempdir()
    for pattern in ['chrome*', 'chromedriver*']:
        for path in glob.glob(os.path.join(tmp_dir, pattern)):
            logging.info("Removing local path: %s", path)
            try:
                if os.path.isdir(path):
                    shutil.rmtree(path, ignore_errors=True)
                else:
                    os.remove(path)
            except OSError:
                pass
    logging.info("Cleaned up local Chrome/Chromedriver directories.")


class PosixSender(Sender):
    """Shared behavior for macOS, Linux, and ChromeOS senders."""

    UNAME = 'uname'
    CHROME_APP = 'chrome'
    CHROMEDRIVER_CHECK_CMD = 'pgrep chromedriver'
    TERMINATE_CHROMEDRIVER_CMD = (
        'pkill -f chromedriver || true; pkill -f chrome || true')
    STATUS_CMD = f'curl -s -o /dev/null -w "%{{http_code}}" {_STATUS_URL}'

    def _console_log_command(self):
        return (f'cat {self.TMP_DIR}/chromedriver_console*.log '
                '2>/dev/null || true')

    def _downloads_cleanup_command(self):
        return f'rm -f {self.TMP_DIR}/*.zip'

    def _binaries_cleanup_command(self):
        return f'rm -rf {self.TMP_DIR}/chrome* {self.TMP_DIR}/chromedriver*'

    def _detect_arch(self, probes):
        arch = self._probe(f'{self.UNAME} -m', probes)
        return 'x64' if arch == 'x86_64' else arch

    def _install(self, chrome_url, driver_url):
        tmp = self.TMP_DIR
        chrome_zip, chrome_dir = _zip_name_and_dir(chrome_url)
        driver_zip, driver_dir = _zip_name_and_dir(driver_url)
        app_path = f"{tmp}/{chrome_dir}/{self.CHROME_APP}"
        driver_path = f"{tmp}/{driver_dir}/chromedriver"

        check = self.run(f"{self._installed_test(app_path, driver_path)} && "
                         "echo 'EXISTS' || echo 'MISSING'")
        if check.stdout.strip() == 'EXISTS':
            logging.info(self._already_installed_message())
        else:
            self.run(f"curl -L {chrome_url} -o {tmp}/{chrome_zip} && "
                     f"curl -L {driver_url} -o {tmp}/{driver_zip} && "
                     f"unzip -o {tmp}/{chrome_zip} -d {tmp} && "
                     f"unzip -o {tmp}/{driver_zip} -d {tmp}")
            self._after_extract(f'{tmp}/{chrome_dir}', f'{tmp}/{driver_dir}')

        self._start_chromedriver(driver_path)
        return app_path

    def _installed_test(self, app_path, driver_path):
        return f"[ -f '{app_path}' ] && [ -f '{driver_path}' ]"

    def _already_installed_message(self):
        return (f"Chrome and Chromedriver already installed on remote "
                f"{self.DISPLAY_NAME}. Skipping download/extract.")

    def _after_extract(self, chrome_dir, driver_dir):
        """Hook for OS-specific fixups after unzipping."""

    def _start_chromedriver(self, driver_path):
        self.run(f'chmod +x {driver_path}')
        self.transport.spawn(f'nohup {driver_path} {_CHROMEDRIVER_FLAGS} '
                             f'> /tmp/chromedriver_console.log 2>&1 &')


class MacSender(PosixSender):
    """macOS sender."""

    OS_NAME = 'mac'
    DISPLAY_NAME = 'Mac'
    # Absolute paths avoid PATH issues in non-interactive SSH sessions.
    UNAME = '/usr/bin/uname'
    CHROME_APP = 'Google Chrome for Testing.app'
    CFT_PLATFORMS = {'arm64': 'mac-arm64', 'x64': 'mac-x64'}
    CHROMEDRIVER_CHECK_CMD = 'ps aux | grep chromedriver | grep -v grep'
    TERMINATE_CHROMEDRIVER_CMD = (
        'killall chromedriver 2>/dev/null || true; '
        'killall "Google Chrome for Testing" 2>/dev/null || true')

    def _detect_os_version(self, probes):
        return self._probe('/usr/bin/sw_vers -productVersion', probes)

    def _installed_test(self, app_path, driver_path):
        # The Chrome app is a bundle directory on macOS.
        return f"[ -d '{app_path}' ] && [ -f '{driver_path}' ]"

    def _after_extract(self, chrome_dir, driver_dir):
        # Clear quarantine attributes so Gatekeeper does not block launch.
        self.run(f"xattr -cr {chrome_dir} && xattr -cr {driver_dir}")


class LinuxSender(PosixSender):
    """Desktop Linux sender."""

    OS_NAME = 'linux'
    DISPLAY_NAME = 'Linux'
    CFT_PLATFORMS = {'x64': 'linux64'}

    def _detect_os_version(self, probes):
        return self._probe('uname -r', probes)


class CrosSender(LinuxSender):
    """ChromeOS DUT. Chrome is driven through Crossbench (see cros_setup)."""

    OS_NAME = 'cros'
    DISPLAY_NAME = 'ChromeOS'
    TMP_DIR = '/usr/local/tmp'
    UNAME = '/usr/bin/uname'

    def _detect_os_version(self, probes):
        return 'cros'

    def _installed_test(self, app_path, driver_path):
        return f"[ -f '{app_path}' ]"

    def _already_installed_message(self):
        return ("Chrome already installed on ChromeOS. "
                "Skipping download/extract.")

    def _start_chromedriver(self, driver_path):
        # Crossbench launches chromedriver itself.
        pass

    def start_monitoring(self, csv_remote_path):
        """Logs battery power draw once a second instead of using glances."""
        # The PID goes to /tmp/cros_power.pid so stop_monitoring can kill it.
        cros_cmd = (
            "sh -c 'echo $$ > /tmp/cros_power.pid; "
            "while true; do "
            "if [ -f /sys/class/power_supply/battery/power_now ]; then "
            "cat /sys/class/power_supply/battery/power_now; "
            "elif [ -f /sys/class/power_supply/sbat0/power_now ]; then "
            "cat /sys/class/power_supply/sbat0/power_now; "
            "else echo 0; fi >> /tmp/cros_power.txt; "
            "sleep 1; done'")
        logging.info("Starting ChromeOS power monitoring in background...")
        return self.transport.spawn(cros_cmd)

    def stop_monitoring(self, monitor_proc, csv_remote_path, csv_local_path):
        logging.info("Stopping Glances/Power monitoring...")
        self.run("if [ -f /tmp/cros_power.pid ]; then "
                 "kill -9 $(cat /tmp/cros_power.pid) 2>/dev/null || true; "
                 "rm -f /tmp/cros_power.pid; "
                 "fi")
        remote_log = "/tmp/cros_power.txt"
        self.copy_from(remote_log, csv_local_path)
        self.run(f"rm -f {remote_log}")


class WindowsSender(Sender):
    """Windows sender. Commands are wrapped in PowerShell."""

    OS_NAME = 'win'
    DISPLAY_NAME = 'Windows'
    TMP_DIR = WIN_REMOTE_TMP_DIR
    CFT_PLATFORMS = {'x64': 'win64', 'x86': 'win32'}
    CHROMEDRIVER_CHECK_CMD = (
        'powershell -Command "Get-Process -Name chromedriver -ErrorAction '
        'SilentlyContinue"')
    TERMINATE_CHROMEDRIVER_CMD = (
        'powershell -Command "Stop-Process -Name chromedriver,chrome -Force '
        '-ErrorAction SilentlyContinue; '
        'taskkill /F /IM chromedriver.exe /IM chrome.exe /T; exit 0"')
    STATUS_CMD = f'curl.exe -s -o NUL -w "%{{http_code}}" {_STATUS_URL}'
    GLANCES_PYTHON = 'python'
    GLANCES_KILL_CMD = (
        'powershell -Command "Get-WmiObject Win32_Process | '
        'Where-Object { $_.CommandLine -like \'*glances*\' } | '
        'ForEach-Object { Stop-Process $_.ProcessId -Force }"')

    # Win32_Processor.Architecture codes.
    _CIM_ARCH = {'0': 'x86', '9': 'x64', '12': 'x64'}  # ARM64 runs x64.
    _ENV_ARCH = {'AMD64': 'x64', 'ARM64': 'x64', 'x86': 'x86'}

    def _console_log_command(self):
        return ('powershell -Command "Get-Content -Path '
                f'{self.TMP_DIR}/chromedriver-win*/chromedriver_console*.log '
                '-ErrorAction SilentlyContinue"')

    def _downloads_cleanup_command(self):
        return ('powershell -Command "Remove-Item -Path '
                f'{self.TMP_DIR}/*.zip '
                '-Force -ErrorAction SilentlyContinue"')

    def _binaries_cleanup_command(self):
        return ('powershell -Command "Remove-Item -Path '
                f'{self.TMP_DIR}/chrome*,{self.TMP_DIR}/chromedriver* '
                '-Recurse -Force -ErrorAction SilentlyContinue"')

    def _detect_arch(self, probes):
        # CIM avoids shell-specific environment issues.
        cim_code = self._probe(
            'powershell -Command '
            '"(Get-CimInstance Win32_Processor).Architecture"', probes)
        if cim_code in self._CIM_ARCH:
            return self._CIM_ARCH[cim_code]
        # Fall back to environment variables if CIM fails. Unknown or empty
        # values stay None rather than guessing x86.
        env_arch = self._probe(
            'powershell -Command "if ($env:PROCESSOR_ARCHITEW6432) '
            '{ $env:PROCESSOR_ARCHITEW6432 } else '
            '{ $env:PROCESSOR_ARCHITECTURE }"', probes)
        return self._ENV_ARCH.get(env_arch)

    def _detect_os_version(self, probes):
        return self._probe(
            'powershell -Command '
            '"[System.Environment]::OSVersion.Version.ToString()"', probes)

    def _install(self, chrome_url, driver_url):
        tmp = self.TMP_DIR
        self.run(f'powershell -Command "if (!(Test-Path \'{tmp}\')) '
                 f'{{ New-Item -ItemType Directory -Path \'{tmp}\' '
                 '-Force }}"')

        chrome_zip, chrome_dir = _zip_name_and_dir(chrome_url)
        driver_zip, driver_dir = _zip_name_and_dir(driver_url)
        chrome_zip_path = f"{tmp}/{chrome_zip}"
        driver_zip_path = f"{tmp}/{driver_zip}"
        app_path = f'{tmp}/{chrome_dir}/chrome.exe'
        driver_path = f'{tmp}/{driver_dir}/chromedriver.exe'

        check = self.run(
            f"powershell -Command \"if ((Test-Path '{app_path}') -and "
            f"(Test-Path '{driver_path}')) "
            f"{{ Write-Output 'EXISTS' }} else {{ Write-Output 'MISSING' }}\"")
        if check.stdout.strip() == 'EXISTS':
            logging.info(self._already_installed_message())
        else:
            logging.info("Downloading and unzipping Chrome/Chromedriver...")
            result = self.run(
                f"powershell -Command \"Set-Variable -Name "
                f"ErrorActionPreference -Value Stop; Set-Variable -Name "
                f"ProgressPreference -Value SilentlyContinue; "
                f"Stop-Process -Name chromedriver,chrome -Force "
                f"-ErrorAction SilentlyContinue; "
                f"Remove-Item -Path '{tmp}/chrome*',"
                f"'{tmp}/chromedriver*' -Recurse -Force "
                f"-ErrorAction SilentlyContinue; "
                f"curl.exe -L '{chrome_url}' -o '{chrome_zip_path}'; "
                f"curl.exe -L '{driver_url}' -o '{driver_zip_path}'; "
                f"if (Test-Path '{WIN_SYSTEM32_TAR}') {{ "
                f"Set-Location '{tmp}'; "
                f"& '{WIN_SYSTEM32_TAR}' -xf '{chrome_zip}'; "
                f"& '{WIN_SYSTEM32_TAR}' -xf '{driver_zip}' "
                f"}} else {{ "
                f"Expand-Archive -Path '{chrome_zip_path}' "
                f"-DestinationPath '{tmp}' -Force; "
                f"Expand-Archive -Path '{driver_zip_path}' "
                f"-DestinationPath '{tmp}' -Force }}\"")
            if result.returncode != 0:
                raise RuntimeError(f"Failed to setup Chrome/Chromedriver on "
                                   f"Windows: {result.stderr}")

        self._start_chromedriver(f'{tmp}/{driver_dir}')
        return app_path

    def _already_installed_message(self):
        return ("Chrome and Chromedriver already installed on Windows. "
                "Skipping download/extract.")

    def _start_chromedriver(self, driver_dir):
        """Starts chromedriver in the interactive session via schtasks.

        chromedriver must run in the logged-in desktop session to open
        windows, which a plain SSH session cannot do.
        """
        batch_script = (
            'DisplaySwitch.exe /external\n'
            f'set PATH=%PATH%;{driver_dir}\n'
            f'cd /d "{driver_dir}"\n'
            f'"{driver_dir}/chromedriver.exe" '
            f'--port={perf_config.CHROMEDRIVER_PORT} '
            '--disable-ipv6 --allowed-origins=* --allowed-ips= --verbose '
            f'--log-path="{driver_dir}/chromedriver_verbose.log" '
            '--enable-chrome-logs > '
            f'"{driver_dir}/chromedriver_console.log" '
            '2>&1\n')
        batch_path = f'{self.TMP_DIR}/start_chromedriver.bat'
        self.run(f"powershell -Command \"'{batch_script}' | "
                 f"Out-File -FilePath '{batch_path}' -Encoding ascii\"")

        # Wrapped in PowerShell so this works whether sshd's shell is cmd,
        # PowerShell, or bash.
        self.run('powershell -Command '
                 '"schtasks /delete /tn StartChromeDriverTask /f"')
        self.run('powershell -Command '
                 '"schtasks /create /tn StartChromeDriverTask /tr '
                 f'\'{batch_path}\' /sc ONCE /st 23:59 /IT /f"')
        self.run('powershell -Command '
                 '"schtasks /run /tn StartChromeDriverTask"')


SENDER_CLASSES = {
    cls.OS_NAME: cls
    for cls in (MacSender, LinuxSender, CrosSender, WindowsSender)
}


def make_sender(args):
    """Returns the Sender for `args.sender_os` on `args.sender`."""
    try:
        sender_class = SENDER_CLASSES[args.sender_os]
    except KeyError:
        raise NotImplementedError(
            f"Unsupported sender_os: {args.sender_os}") from None
    return sender_class(
        remote_transport.make_transport(args.sender, args.username))
