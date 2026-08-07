# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A common module for media performance tests."""

import glob
import json
import logging
import multiprocessing
import os
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.request

from contextlib import AbstractContextManager

# pylint: disable=import-error, wrong-import-position
REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))
BUILD_UTIL_ROOT = os.path.join(REPO_ROOT, 'build', 'util')
sys.path.append(BUILD_UTIL_ROOT)
from lib.proto import measures
from lib.results import result_sink

CHROME_FUCHSIA_ROOT = os.path.join(REPO_ROOT, 'fuchsia_web', 'av_testing')
sys.path.append(CHROME_FUCHSIA_ROOT)
import server  # pylint: disable=unused-import
import video_analyzer  # pylint: disable=unused-import
import camera  # pylint: disable=unused-import

TEST_SCRIPTS_ROOT = os.path.join(REPO_ROOT, 'build', 'fuchsia', 'test')
sys.path.append(TEST_SCRIPTS_ROOT)
from repeating_log import RepeatingLog  # pylint: disable=unused-import
# pylint: enable=import-error, wrong-import-position

# --- Chrome for Testing Constants ---
CFT_JSON_URL = ("https://googlechromelabs.github.io/chrome-for-testing/"
                "known-good-versions-with-downloads.json")

CHROMEDRIVER_PORT = int(os.environ.get('CHROMEDRIVER_PORT', '49573'))
SERVER_PORT = int(os.environ.get('SERVER_PORT', '8000'))

RECORDINGS_DIR = os.path.join(os.environ.get('ISOLATED_OUTDIR', '/tmp'),
                              'recordings')
TRACES_DIR = os.path.join(os.environ.get('ISOLATED_OUTDIR', '/tmp'),
                          'traces')
LOCAL_HOST_IP = '127.0.0.1'
REMOTE_URL = f'http://{LOCAL_HOST_IP}:{CHROMEDRIVER_PORT}'

# This code is used as the default failure value for recordings in the case that
# `results.get()` throws an unexpected error. -128 is chosen as a clear fail
# case (large negative) that won't overly distort tracking graphs.
FAIL_CODE = -128

METRICS = [
    'smoothness',
    'freezing',
    'dropped_frame_count',
    'total_frame_count',
    'dropped_frame_percentage'
]

# Framerate is now legacy data, but until our results are standardized we'll
# maintain the data in case it's necessary to pass later.
VIDEOS = [
    {
        'name': '1080p30fpsAV1_foodmarket_sync.mp4',
        'fps': 30
    },
    {
        'name': '1080p30fpsH264_foodmarket_yt_sync.mp4',
        'fps': 30
    },
    {
        'name': '1080p60fpsHEVC_boat_yt_sync.mp4',
        'fps': 60
    },
    {
        'name': '1080p60fpsVP9_boat_yt_sync.webm',
        'fps': 60
    }
]


SENDER_CHROMEDRIVER_CHECK_CMD = {
    'mac': (
        'ps aux | grep chromedriver | grep -v grep'
    ),
    'win': (
        'powershell -Command "Get-Process -Name chromedriver -ErrorAction '
        'SilentlyContinue"'
    ),
    'linux': (
        'pgrep chromedriver'
    ),
    'cros': (
        'pgrep chromedriver'
    ),
}

SENDER_STATUS_CMD = {
    'mac': (
        'curl -s -o /dev/null -w "%{http_code}" '
        f'http://{LOCAL_HOST_IP}:{CHROMEDRIVER_PORT}/status'
    ),
    'win': (
        f'curl.exe -s -o NUL -w "%{{http_code}}" '
        f'http://{LOCAL_HOST_IP}:{CHROMEDRIVER_PORT}/status'
    ),
    'linux': (
        'curl -s -o /dev/null -w "%{http_code}" '
        f'http://{LOCAL_HOST_IP}:{CHROMEDRIVER_PORT}/status'
    ),
    'cros': (
        'curl -s -o /dev/null -w "%{http_code}" '
        f'http://{LOCAL_HOST_IP}:{CHROMEDRIVER_PORT}/status'
    ),
}

SENDER_TERMINATE_DRIVER_CMD = {
    'mac': (
        'killall chromedriver 2>/dev/null || true; '
        'killall "Google Chrome for Testing" 2>/dev/null || true'
    ),
    'win': (
        'powershell -Command "Stop-Process -Name chromedriver,chrome -Force '
        '-ErrorAction SilentlyContinue; '
        'taskkill /F /IM chromedriver.exe /IM chrome.exe /T; exit 0"'
    ),
    'linux': (
        'pkill -f chromedriver || true; pkill -f chrome || true'
    ),
    'cros': (
        'pkill -f chromedriver || true; pkill -f chrome || true'
    ),
}


WIN_REMOTE_TMP_DIR = 'C:/cft_temp'
WIN_SYSTEM32_TAR = 'C:/Windows/System32/tar.exe'


class StartProcess(AbstractContextManager):
    """Starts a multiprocessing.Process."""

    def __init__(self, target, args, terminate: bool):
        self._proc = multiprocessing.Process(target=target, args=args)
        self._terminate = terminate

    def __enter__(self):
        self._proc.start()

    def __exit__(self, exc_type, exc_value, traceback):
        if self._terminate:
            self._proc.terminate()
        self._proc.join()
        if not self._terminate:
            assert self._proc.exitcode == 0


def send_ssh_command(hostname, username, command, blocking=False):
    """
    Sends a command to a host. If hostname is 'localhost' or None, it runs
    locally. Otherwise, it uses SSH.

    Args:
        hostname (str): The host to connect to.
        username (str): The username for the SSH connection.
        command (str): The command to execute.
        blocking (bool): If True, waits for the command to complete.
                         If False, runs the command in a non-blocking way.

    Returns:
        subprocess.CompletedProcess or subprocess.Popen: The process object.
    """
    if hostname in ['localhost', '127.0.0.1', None]:
        logging.debug('Executing local command: %s', command)
        if blocking:
            return subprocess.run(command,
                                    shell=True,
                                    capture_output=True,
                                    text=True,
                                    timeout=120,
                                    check=False)
        return subprocess.Popen(command,
                                 shell=True,
                                 stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE,
                                 text=True)

    key_path = os.path.expanduser('~/.ssh/id_ed25519')
    ssh_command = [
        'ssh',
        '-o', 'StrictHostKeyChecking=no',
        '-i', key_path,
        f'{username}@{hostname}',
        command
    ]
    logging.debug('Executing SSH command: %s', ' '.join(ssh_command))

    if blocking:
        process = subprocess.run(ssh_command,
                                 capture_output=True,
                                 text=True,
                                 timeout=120,
                                 check=False)
    else:
        process = subprocess.Popen(  # pylint: disable=consider-using-with
            ssh_command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True)

    return process


def terminate_old_chromedriver(args):
    """Tries to terminate any existing chromedriver processes."""
    logging.info("Attempting to terminate old chromedriver processes...")
    send_ssh_command(args.sender, args.username,
                     SENDER_TERMINATE_DRIVER_CMD[args.sender_os],
                     blocking=True)

    for _ in range(15):
        result = send_ssh_command(args.sender,
                                  args.username,
                                  SENDER_CHROMEDRIVER_CHECK_CMD[args.sender_os],
                                  blocking=True)
        if not result.stdout.strip():
            logging.info("Old chromedriver processes confirmed gone.")
            return
        logging.info("Old chromedriver processes still present, waiting...")
        time.sleep(1)
    raise RuntimeError("Chromedriver processes lingered after kill attempts.")


def get_remote_info(args):
    """Detects info (arch, OS version) of the machine."""
    if args.sender in ['localhost', '127.0.0.1', None]:
        import platform
        arch = platform.machine()
        info = {
            'arch': 'x64' if arch == 'x86_64' else arch,
            'os_version': platform.release()
        }
        return info

    info = {'arch': None, 'os_version': None}
    if args.sender_os == 'mac':
        # Use absolute paths on Mac to avoid PATH issues in non-interactive SSH.
        arch_result = send_ssh_command(args.sender,
                                       args.username,
                                       '/usr/bin/uname -m',
                                       blocking=True)
        arch = arch_result.stdout.strip()
        info['arch'] = 'x64' if arch == 'x86_64' else arch

        version_result = send_ssh_command(args.sender,
                                          args.username,
                                          '/usr/bin/sw_vers -productVersion',
                                          blocking=True)
        info['os_version'] = version_result.stdout.strip()

    elif args.sender_os == 'cros':
        # Standard uname for architecture.
        arch_result = send_ssh_command(args.sender,
                                       args.username,
                                       '/usr/bin/uname -m',
                                       blocking=True)
        arch = arch_result.stdout.strip()
        info['arch'] = 'x64' if arch == 'x86_64' else arch
        info['os_version'] = 'cros'

    elif args.sender_os == 'win':
        # Get architecture using CIM to avoid shell-specific environment issues.
        arch_cmd = (
            'powershell -Command '
            '"(Get-CimInstance Win32_Processor).Architecture"'
        )
        arch_result = send_ssh_command(args.sender,
                                       args.username,
                                       arch_cmd,
                                       blocking=True)
        # Architecture codes: 0 = x86, 9 = x64, 12 = ARM64
        arch_code = arch_result.stdout.strip()
        if arch_code == '9':
            info['arch'] = 'x64'
        elif arch_code == '12':
            info['arch'] = 'x64'  # Map ARM64 to x64 for emulation
        elif arch_code == '0':
            info['arch'] = 'x86'
        else:
            # Fallback to environment variables if CIM fails
            arch_cmd_fallback = (
                'powershell -Command "if ($env:PROCESSOR_ARCHITEW6432) '
                '{ $env:PROCESSOR_ARCHITEW6432 } else '
                '{ $env:PROCESSOR_ARCHITECTURE }"'
            )
            arch_result = send_ssh_command(args.sender,
                                           args.username,
                                           arch_cmd_fallback,
                                           blocking=True)
            arch = arch_result.stdout.strip()
            info['arch'] = 'x64' if arch in ['AMD64', 'ARM64'] else 'x86'

        version_result = send_ssh_command(
            args.sender,
            args.username,
            'powershell -Command '
            '"[System.Environment]::OSVersion.Version.ToString()"',
            blocking=True)
        info['os_version'] = version_result.stdout.strip()

    elif args.sender_os == 'linux':
        arch_result = send_ssh_command(args.sender,
                                       args.username,
                                       'uname -m',
                                       blocking=True)
        arch = arch_result.stdout.strip()
        info['arch'] = 'x64' if arch == 'x86_64' else arch

        version_result = send_ssh_command(args.sender,
                                          args.username,
                                          'uname -r',
                                          blocking=True)
        info['os_version'] = version_result.stdout.strip()

    return info


def download_cft_urls(platform_name, version=None):
    """
    Downloads the CfT JSON and finds the URLs for a specific version.
    """
    logging.info("Downloading Chrome for Testing JSON data...")
    with urllib.request.urlopen(CFT_JSON_URL) as url:
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


def install_and_setup_chrome(args, chrome_version):
    """
    Downloads and sets up a specific version of Chrome for Testing and its
    matching chromedriver.
    """
    info = get_remote_info(args)
    arch = info['arch']
    os_version = info['os_version']
    logging.info("Detected remote info: %s", info)

    platform_map = {
        'mac': {
            'arm64': 'mac-arm64',
            'x64': 'mac-x64'
        },
        'win': {
            'x64': 'win64',
            'x86': 'win32'
        },
        'linux': {
            'x64': 'linux64'
        },
        'cros': {
            'x64': 'linux64'
        }
    }

    if args.sender_os not in platform_map or arch not in platform_map[
            args.sender_os]:
        raise NotImplementedError(
            f"Unsupported OS/Arch: {args.sender_os}/{arch}")

    platform_name = platform_map[args.sender_os][arch]
    chrome_version_actual, chrome_url, driver_url = download_cft_urls(
        platform_name, chrome_version)
    remote_app_path = None

    # --- Download and Unzip ---
    logging.info("Downloading Chrome and Chromedriver.")
    if args.sender in ['localhost', '127.0.0.1', None]:
        # Handle local installation on the NUC.
        tmp_dir = '/tmp'
        chrome_zip = chrome_url.split('/')[-1]
        driver_zip = driver_url.split('/')[-1]

        chrome_dir = chrome_zip.replace('.zip', '')
        driver_dir = driver_zip.replace('.zip', '')
        remote_app_path = (f"{tmp_dir}/{chrome_dir}/"
                           "Google Chrome for Testing.app")
        if sys.platform == 'linux':
             remote_app_path = f"{tmp_dir}/{chrome_dir}/chrome"

        remote_chromedriver_path = f"{tmp_dir}/{driver_dir}/chromedriver"

        if (os.path.exists(remote_app_path)
                and os.path.exists(remote_chromedriver_path)):
             logging.info(
                 "Chrome and Chromedriver already installed locally. "
                 "Skipping download/extract.")
        else:
             subprocess.run(
                 f"curl -L {chrome_url} -o {tmp_dir}/{chrome_zip} && "
                 f"curl -L {driver_url} -o {tmp_dir}/{driver_zip} && "
                 f"unzip -o {tmp_dir}/{chrome_zip} -d {tmp_dir} && "
                 f"unzip -o {tmp_dir}/{driver_zip} -d {tmp_dir}",
                 shell=True, check=True, timeout=120)

        subprocess.run(f'chmod +x {remote_chromedriver_path}',
                       shell=True, check=True)
        # Start chromedriver locally.
        subprocess.Popen(
            f'nohup {remote_chromedriver_path} --port={CHROMEDRIVER_PORT} '
            '--disable-ipv6 --allowed-origins=\"*\" --allowed-ips= '
            '--verbose --log-path=/tmp/chromedriver_verbose.log '
            '--enable-chrome-logs '
            f'> /tmp/chromedriver_console.log 2>&1 &', shell=True)

        logging.info("Finished local chromedriver setup.")
        return remote_app_path, chrome_version_actual

    if args.sender_os == 'mac':
        remote_tmp_dir = '/tmp'
        chrome_zip = chrome_url.split('/')[-1]
        driver_zip = driver_url.split('/')[-1]
        chrome_dir = chrome_zip.replace('.zip', '')
        driver_dir = driver_zip.replace('.zip', '')
        remote_app_path = (
            f"{remote_tmp_dir}/{chrome_dir}/Google "
            "Chrome for Testing.app")
        remote_chromedriver_path = (
            f"{remote_tmp_dir}/{driver_dir}/chromedriver")

        check_installed_cmd = (
            f"[ -d '{remote_app_path}' ] && "
            f"[ -f '{remote_chromedriver_path}' ] && "
            "echo 'EXISTS' || echo 'MISSING'"
        )
        check_result = send_ssh_command(
            args.sender, args.username, check_installed_cmd, blocking=True)
        if check_result.stdout.strip() == 'EXISTS':
            logging.info(
                "Chrome and Chromedriver already installed on remote Mac. "
                "Skipping download/extract.")
        else:
            send_ssh_command(
                args.sender, args.username,
                (f"curl -L {chrome_url} -o {remote_tmp_dir}/{chrome_zip} && "
                 f"curl -L {driver_url} -o {remote_tmp_dir}/{driver_zip} && "
                 f"unzip -o {remote_tmp_dir}/{chrome_zip} "
                 f"-d {remote_tmp_dir} && "
                 f"unzip -o {remote_tmp_dir}/{driver_zip} "
                 f"-d {remote_tmp_dir}"),
                blocking=True)

            send_ssh_command(
                args.sender, args.username,
                (f"xattr -cr {remote_tmp_dir}/{chrome_dir} && "
                 f"xattr -cr {remote_tmp_dir}/{driver_dir}"),
                blocking=True)

        send_ssh_command(args.sender, args.username,
                         f'chmod +x {remote_chromedriver_path}',
                         blocking=True)
        send_ssh_command(
            args.sender, args.username,
            (f'nohup {remote_chromedriver_path} --port={CHROMEDRIVER_PORT} '
             '--disable-ipv6 --allowed-origins=\"*\" --allowed-ips= '
             '--verbose --log-path=/tmp/chromedriver_verbose.log '
             '--enable-chrome-logs '
             f'> /tmp/chromedriver_console.log 2>&1 &'))

    elif args.sender_os == 'linux':
        remote_tmp_dir = '/tmp'
        chrome_zip = chrome_url.split('/')[-1]
        driver_zip = driver_url.split('/')[-1]
        chrome_dir = chrome_zip.replace('.zip', '')
        driver_dir = driver_zip.replace('.zip', '')
        remote_app_path = f"{remote_tmp_dir}/{chrome_dir}/chrome"
        remote_chromedriver_path = f"{remote_tmp_dir}/{driver_dir}/chromedriver"

        check_installed_cmd = (
            f"[ -f '{remote_app_path}' ] && "
            f"[ -f '{remote_chromedriver_path}' ] && "
            "echo 'EXISTS' || echo 'MISSING'"
        )
        check_result = send_ssh_command(
            args.sender, args.username, check_installed_cmd, blocking=True)
        if check_result.stdout.strip() == 'EXISTS':
            logging.info(
                "Chrome and Chromedriver already installed on remote Linux. "
                "Skipping download/extract.")
        else:
            send_ssh_command(
                args.sender, args.username,
                (f"curl -L {chrome_url} -o {remote_tmp_dir}/{chrome_zip} && "
                 f"curl -L {driver_url} -o {remote_tmp_dir}/{driver_zip} && "
                 f"unzip -o {remote_tmp_dir}/{chrome_zip} "
                 f"-d {remote_tmp_dir} && "
                 f"unzip -o {remote_tmp_dir}/{driver_zip} "
                 f"-d {remote_tmp_dir}"),
                blocking=True)

        send_ssh_command(args.sender, args.username,
                         f'chmod +x {remote_chromedriver_path}',
                         blocking=True)
        send_ssh_command(
            args.sender, args.username,
            (f'nohup {remote_chromedriver_path} --port={CHROMEDRIVER_PORT} '
             '--disable-ipv6 --allowed-origins=\"*\" --allowed-ips= '
             '--verbose --log-path=/tmp/chromedriver_verbose.log '
             '--enable-chrome-logs '
             f'> /tmp/chromedriver_console.log 2>&1 &'))

    elif args.sender_os == 'cros':
        remote_tmp_dir = '/usr/local/tmp'
        chrome_zip = chrome_url.split('/')[-1]
        driver_zip = driver_url.split('/')[-1]
        chrome_dir = chrome_zip.replace('.zip', '')
        driver_dir = driver_zip.replace('.zip', '')
        remote_app_path = f"{remote_tmp_dir}/{chrome_dir}/chrome"

        check_installed_cmd = (
            f"[ -f '{remote_app_path}' ] && echo 'EXISTS' || echo 'MISSING'"
        )
        check_result = send_ssh_command(
            args.sender, args.username, check_installed_cmd, blocking=True)
        if check_result.stdout.strip() == 'EXISTS':
            logging.info(
                "Chrome already installed on ChromeOS. "
                "Skipping download/extract.")
        else:
            send_ssh_command(
                args.sender, args.username,
                (f"curl -L {chrome_url} -o {remote_tmp_dir}/{chrome_zip} && "
                 f"curl -L {driver_url} -o {remote_tmp_dir}/{driver_zip} && "
                 f"unzip -o {remote_tmp_dir}/{chrome_zip} "
                 f"-d {remote_tmp_dir} && "
                 f"unzip -o {remote_tmp_dir}/{driver_zip} "
                 f"-d {remote_tmp_dir}"),
                blocking=True)

    elif args.sender_os == 'win':
        remote_tmp_dir = WIN_REMOTE_TMP_DIR
        # Use shell-agnostic PowerShell for directory creation
        send_ssh_command(
            args.sender,
            args.username,
            f'powershell -Command "if (!(Test-Path \'{remote_tmp_dir}\')) '
            '{{ New-Item -ItemType Directory -Path \'{remote_tmp_dir}\' '
            '-Force }}"',
            blocking=True)

        chrome_zip_name = chrome_url.split('/')[-1]
        driver_zip_name = driver_url.split('/')[-1]
        chrome_zip_path = f"{remote_tmp_dir}/{chrome_zip_name}"
        driver_zip_path = f"{remote_tmp_dir}/{driver_zip_name}"
        chrome_dir = chrome_zip_name.replace('.zip', '')
        driver_dir = driver_zip_name.replace('.zip', '')
        remote_app_path = f'{remote_tmp_dir}/{chrome_dir}/chrome.exe'
        remote_chromedriver_path = (
            f'{remote_tmp_dir}/{driver_dir}/chromedriver.exe'
        )

        check_installed_cmd = (
            f"powershell -Command \"if ((Test-Path '{remote_app_path}') -and "
            f"(Test-Path '{remote_chromedriver_path}')) "
            f"{{ Write-Output 'EXISTS' }} else {{ Write-Output 'MISSING' }}\""
        )
        check_result = send_ssh_command(
            args.sender, args.username, check_installed_cmd, blocking=True)
        if check_result.stdout.strip() == 'EXISTS':
            logging.info(
                "Chrome and Chromedriver already installed on Windows. "
                "Skipping download/extract.")
        else:
            # Download and Unzip using a single robust PowerShell command
            logging.info("Downloading and unzipping Chrome/Chromedriver...")
            setup_cmd = (
                f"powershell -Command \"Set-Variable -Name "
                f"ErrorActionPreference -Value Stop; Set-Variable -Name "
                f"ProgressPreference -Value SilentlyContinue; "
                f"Stop-Process -Name chromedriver,chrome -Force "
                f"-ErrorAction SilentlyContinue; "
                f"Remove-Item -Path '{remote_tmp_dir}/chrome*',"
                f"'{remote_tmp_dir}/chromedriver*' -Recurse -Force "
                f"-ErrorAction SilentlyContinue; "
                f"curl.exe -L '{chrome_url}' -o '{chrome_zip_path}'; "
                f"curl.exe -L '{driver_url}' -o '{driver_zip_path}'; "
                f"if (Test-Path '{WIN_SYSTEM32_TAR}') {{ "
                f"Set-Location '{remote_tmp_dir}'; "
                f"& '{WIN_SYSTEM32_TAR}' -xf '{chrome_zip_name}'; "
                f"& '{WIN_SYSTEM32_TAR}' -xf '{driver_zip_name}' "
                f"}} else {{ "
                f"Expand-Archive -Path '{chrome_zip_path}' "
                f"-DestinationPath '{remote_tmp_dir}' -Force; "
                f"Expand-Archive -Path '{driver_zip_path}' "
                f"-DestinationPath '{remote_tmp_dir}' -Force }}\""
            )
            result = send_ssh_command(args.sender, args.username, setup_cmd,
                                      blocking=True)
            if result.returncode != 0:
                raise RuntimeError(f"Failed to setup Chrome/Chromedriver on "
                                   f"Windows: {result.stderr}")

        # Create and run the batch script
        batch_script_content = (
            'DisplaySwitch.exe /external\n'
            f'set PATH=%PATH%;{remote_tmp_dir}/{driver_dir}\n'
            f'cd /d "{remote_tmp_dir}/{driver_dir}"\n'
            f'"{remote_tmp_dir}/{driver_dir}/chromedriver.exe" '
            f'--port={CHROMEDRIVER_PORT} '
            '--disable-ipv6 --allowed-origins=* --allowed-ips= --verbose '
            f'--log-path="{remote_tmp_dir}/{driver_dir}/'
            'chromedriver_verbose.log" '
            '--enable-chrome-logs > '
            f'"{remote_tmp_dir}/{driver_dir}/chromedriver_console.log" '
            '2>&1\n'
        )

        batch_script_path = f'{remote_tmp_dir}/start_chromedriver.bat'
        send_ssh_command(
            args.sender, args.username,
            (f"powershell -Command \"'{batch_script_content}' | "
             f"Out-File -FilePath '{batch_script_path}' "
             '-Encoding ascii"'),
            blocking=True)

        # Schedule and run task (wrapped in PowerShell to handle bash shells)
        send_ssh_command(args.sender, args.username,
                         'powershell -Command '
                         '"schtasks /delete /tn StartChromeDriverTask /f"',
                         blocking=True)
        send_ssh_command(
            args.sender, args.username,
            (f'powershell -Command '
              '"schtasks /create /tn StartChromeDriverTask /tr '
             f'\'{batch_script_path}\' /sc ONCE /st 23:59 /IT /f"'),
            blocking=True)
        send_ssh_command(args.sender, args.username,
                         'powershell -Command '
                         '"schtasks /run /tn StartChromeDriverTask"',
                         blocking=True)
    else:
        raise NotImplementedError(
            f"Unsupported sender_os for install: {args.sender_os}")

    logging.info("Finished chromedriver setup attempt.")
    return remote_app_path, chrome_version_actual


def dump_remote_logs(args, chrome_version=None, codec_name=None):
    """Tries to dump the remote Chromedriver console logs to the local log.

    Note: `chrome_version` and `codec_name` are kept for backwards compatibility
    with callers; all console log files matching wildcard patterns in the temp
    directory are dumped regardless of version or codec parameters.
    """
    _ = (chrome_version, codec_name)
    logging.error("Dumping remote console logs:")

    if args.sender_os == 'win':
        log_path = (
            f"{WIN_REMOTE_TMP_DIR}/chromedriver-win*/"
            "chromedriver_console*.log"
        )
        log_cmd = (
            'powershell -Command "Get-Content -Path '
            f'{log_path} -ErrorAction SilentlyContinue"'
        )
    else:
        tmp_dir = '/usr/local/tmp' if args.sender_os == 'cros' else '/tmp'
        log_cmd = f'cat {tmp_dir}/chromedriver_console*.log 2>/dev/null || true'

    log_result = send_ssh_command(args.sender,
                                  args.username,
                                  log_cmd,
                                  blocking=True)
    if log_result.stdout.strip() or log_result.stderr.strip():
        logging.error("REMOTE CONSOLE LOG:\nSTDOUT: %s\nSTDERR: %s",
                      log_result.stdout, log_result.stderr)


def wait_for_chromedriver(args, chrome_version=None, codec_name=None):
    """Waits for the new chromedriver to be ready by checking its status URL."""
    logging.info("Starting Chromedriver status check...")
    for i in range(30):
        try:
            result = send_ssh_command(args.sender,
                                      args.username,
                                      SENDER_STATUS_CMD[args.sender_os],
                                      blocking=True)
            stdout = result.stdout.strip()
            if result.returncode == 0 and stdout == '200':
                logging.info("Chromedriver is ready.")
                return
            logging.warning("Attempt %d failed. Chromedriver not ready. "
                            "Return code: %d, stdout: '%s', stderr: '%s'",
                            i + 1, result.returncode, stdout,
                            result.stderr.strip())
        except subprocess.TimeoutExpired:
            logging.warning("Status check timed out. Retrying...")
        except Exception as e:  # pylint: disable=broad-exception-caught
            logging.warning("A script-level error occurred: %s. Retrying...", e)
        time.sleep(2)

    # If we reached here, Chromedriver failed to start. Try to dump logs.
    logging.error("Chromedriver failed to start.")
    dump_remote_logs(args, chrome_version, codec_name)
    raise RuntimeError("Chromedriver still not ready after multiple attempts.")

def start_ssh_tunnel(args):
    """Starts the SSH tunnel process."""
    if args.sender in ['localhost', '127.0.0.1', None]:
        logging.info("Local sender detected. Skipping SSH tunnel.")
        return None

    # pylint: disable=consider-using-with
    host_tunnel_cmd = [
        'ssh',
        '-i',
        '~/.ssh/id_ed25519',
        # Optimization for tunnel throughput. Disable compression as video
        # data is already compressed.
        '-o', 'Compression=no',
        '-o', 'ServerAliveInterval=10',
        '-o', 'ServerAliveCountMax=10',
        '-o', 'TCPKeepAlive=yes',
        '-o', 'ExitOnForwardFailure=yes',
        '-L',
        f'{CHROMEDRIVER_PORT}:{LOCAL_HOST_IP}:{CHROMEDRIVER_PORT}',
        '-R',
        f'{SERVER_PORT}:{LOCAL_HOST_IP}:{SERVER_PORT}',
        f'{args.username}@{args.sender}',
        '-N'
    ]
    tunnel_proc = subprocess.Popen(host_tunnel_cmd)
    logging.info("Started tunnel.")
    return tunnel_proc

def teardown_recording_process(rec_proc):
    """
    Tears down the recording process.

    This function safely tears down the ffmpeg recording process via either
    a graceful wait or a forceful terminate.

    Args:
        rec_proc (subprocess.Popen): The video recording process.
    """
    if rec_proc is not None:
        logging.info("Waiting for recording to finish...")
        try:
            rec_proc.communicate(timeout=20)
            logging.info("Recording finished.")
        except subprocess.TimeoutExpired as e:
            logging.warning("WARNING: Recording process timed out after 20 "
                            "seconds. Terminating it now.")
            rec_proc.terminate()
            rec_proc.wait()
            raise RuntimeError("Recording process timed out and was "
                               "forcefully terminated.") from e

def teardown_test_environment(driver, tunnel_proc, args):
    """
    Tears down the test environment, ensuring the driver and tunnel are safely
    terminated.

    This function safely terminates the the Selenium WebDriver, and the SSH
    tunnel. It handles timeouts gracefully and ensures resources are released
    properly.

    Args:
        driver (webdriver.Remote): The Selenium WebDriver instance.
        tunnel_proc (subprocess.Popen): The SSH tunnel process.
        args: The parsed command-line arguments.
    """
    if driver:
        driver.quit()
        logging.info("Terminated chromedriver.")

    if tunnel_proc:
        if hasattr(tunnel_proc, 'poll'):
            if tunnel_proc.poll() is None:
                tunnel_proc.terminate()
                logging.info("Terminated tunnel.")
        elif hasattr(tunnel_proc, 'stop'):
            # Handle Crossbench platform objects.
            tunnel_proc.stop()
            logging.info("Stopped Crossbench platform.")

    cleanup_command = {
        'mac': (
            "rm -f /tmp/*.zip"
        ),
        'win': (
            'powershell -Command "Remove-Item -Path '
            f'{WIN_REMOTE_TMP_DIR}/*.zip '
            '-Force -ErrorAction SilentlyContinue"'
        ),
        'linux': (
            "rm -f /tmp/*.zip"
        ),
        'cros': (
            "rm -f /usr/local/tmp/*.zip"
        ),
    }

    send_ssh_command(args.sender, args.username,
                     cleanup_command[args.sender_os])
    logging.info("Cleaned up tmp files on remote machine.")


def setup_cros_environment(args, chrome_version, chrome_options_list):
    """Sets up ChromeOS environment using Crossbench."""
    logging.info("Setting up ChromeOS environment using Crossbench.")

    # 1. Dynamically mock missing optional dependencies and their submodules.
    from unittest.mock import MagicMock
    import importlib.abc
    import importlib.util

    class MockFinder(importlib.abc.MetaPathFinder):
        def __init__(self, mocked_packages):
            self.mocked_packages = mocked_packages
        def find_spec(self, fullname, path, target=None):
            if any(fullname == pkg or fullname.startswith(pkg + '.')
                   for pkg in self.mocked_packages):
                return importlib.util.spec_from_loader(fullname, self)
            return None
        def create_module(self, spec):
            mock = MagicMock()
            # Ensure the mock is treated as a package for submodule imports.
            mock.__path__ = []
            return mock
        def exec_module(self, module):
            pass

    # Install the finder to handle imports automatically.
    sys.meta_path.insert(0, MockFinder([
        "google.cloud", "psutil", "xlsxwriter", "hjson",
        "mobly", "snippet_uiautomator"
    ]))

    sys.path.insert(0, os.path.join(REPO_ROOT, 'third_party', 'crossbench'))

    from crossbench.plt.chromeos_ssh import ChromeOsSshPlatform
    from crossbench.plt import PLATFORM as host_platform
    from crossbench.browsers.chrome.webdriver import ChromeWebDriverChromeOsSsh
    from crossbench.browsers.settings import Settings
    from crossbench.browsers.viewport import Viewport

    # 2. Initialize Platform and purge zombies.
    cb_platform = ChromeOsSshPlatform(
        host_platform,
        host=args.sender,
        port=0,
        ssh_port=22,
        ssh_user=args.username)

    # Enable detailed logging for Crossbench to debug autologin issues.
    logging.getLogger('crossbench').setLevel(logging.DEBUG)

    # Aggressively clear any leaked sessions.
    logging.info("Purging stale Chrome processes on device...")
    cb_platform.sh("pkill", "-9", "chrome", check=False)

    # 3. Detect remote version to find a matching local driver.
    try:
        # Use platform.app_version for cleaner version detection.
        version_str = cb_platform.app_version("/opt/google/chrome/chrome")
        import re
        version_match = re.search(r'(\d+\.\d+\.\d+\.\d+)', version_str)
        if version_match:
            actual_version = version_match.group(1)
        else:
            # Fallback to the previous split logic if regex fails.
            actual_version = version_str.split()[-2]

        # Use milestone (e.g. '129') to match local driver to remote browser.
        milestone = actual_version.split('.')[0]
        logging.info("Detected remote Chrome version: %s (milestone: %s)",
                     actual_version, milestone)
    except Exception as e:
        logging.warning("Failed to detect remote Chrome version: %s", e)
        milestone = None

    # 4. Set up the chromedriver bridge on the remote device.
    # Crossbench's SSH platform expects the driver to be on the remote device.
    try:
        if not milestone:
            logging.warning("Milestone is None. download_cft_urls will fall "
                            "back to the latest version, which may cause a "
                            "mismatch.")
        _, _, driver_url = download_cft_urls('linux64', milestone)
        install_and_setup_chrome(args, milestone)
        # On ChromeOS, common.py unzips into /usr/local/tmp.
        driver_dir = driver_url.split('/')[-1].replace('.zip', '')
        remote_driver_path = f"/usr/local/tmp/{driver_dir}/chromedriver"
    except Exception as e:
        logging.warning("Failed to install matching ChromeDriver: %s", e)
        remote_driver_path = None

    # 5. Create high-level Browser object.
    # Crossbench requires flags to be split into (name, value) tuples.
    chrome_os_flags = []
    # Strict list of flags to exclude from launch to avoid autologin crashes.
    EXCLUDE_FLAGS = [
        '--window-size', '--window-position', '--start-maximized',
        '--start-fullscreen'
    ]
    for flag in chrome_options_list:
        # Skip geometry flags as they are handled by the Viewport object.
        if any(f in flag for f in EXCLUDE_FLAGS):
            continue
        if '=' in flag:
            chrome_os_flags.append(tuple(flag.split('=', 1)))
        else:
            chrome_os_flags.append(flag)

    # Force the window to launch in a maximized state.
    chrome_os_flags.append(("--window-state", "maximized"))
    chrome_os_flags.append(('--log-file', '/tmp/chrome_debug.log'))

    # Use Viewport.MAXIMIZED for standard ChromeOS window management.
    settings = Settings(
        flags=chrome_os_flags,
        platform=cb_platform,
        driver_path=remote_driver_path,
        viewport=Viewport.MAXIMIZED)

    # We must explicitly provide the binary path on ChromeOS.
    browser = ChromeWebDriverChromeOsSsh(
        label="cros_perf_test",
        path="/opt/google/chrome/chrome",
        settings=settings)

    # Crossbench filters out geometry flags by default for ChromeOS in
    # '_filter_flags_for_run'. We override this behavior to ensure our flags
    # reach the launch script (autologin.py).
    browser.UNSUPPORTED_FLAGS += ("--user-data-dir",)

    # 6. Start the browser with a robust session mock.
    logging.info("Starting Crossbench Browser on ChromeOS...")
    # Mocking the session/run group requirement for start()
    mock_session = MagicMock()
    mock_session.timing.timeout_unit = None
    mock_session.out_dir = RECORDINGS_DIR
    # Prevent Crossbench from trying to use mock secrets for login.
    mock_session.browser.secrets.google = None

    # Crossbench requires the network to be 'open' before starting the browser.
    with browser.network.open(mock_session):
        # Set up reverse port forwarding for the local HTTP server so the remote
        # browser can reach the host machine's port.
        logging.info("Setting up reverse port forwarding for port %d...",
                     SERVER_PORT)
        cb_platform.ports.reverse_forward(
            SERVER_PORT, SERVER_PORT)
        logging.info("Final ChromeOS flags: %s", chrome_os_flags)
        try:
            browser.start(mock_session)
        except Exception as e:
            logging.error(
                "Browser failed to start.")
            raise e

    # ACCESSING PRIVATE DRIVER: Crossbench does not expose the Selenium driver
    # as a public attribute. We use _private_driver to continue using existing
    # Selenium-based test logic.
    driver = browser._private_driver

    # Retrieve actual version for result tagging.
    actual_version = str(browser.version)
    logging.info("Detected remote Chrome version: %s", actual_version)

    return driver, cb_platform, actual_version


def calculate_psnr_ssim(video_file: str,
                        recorded_path: str,
                        original_path: str):
    """Calculates PSNR and SSIM via FFmpeg and records them."""
    import re

    logging.info("Calculating PSNR and SSIM via FFmpeg...")

    # PSNR command to compare the recorded video against the original reference.
    # Args:
    # - '-i': Input files (recorded_path, original_path).
    # - '-lavfi': Libavfilter graph.
    # - '[0:v][1:v]scale2ref[rec][orig]': Scale the recorded video (0:v) to
    #   match the reference video (1:v) resolution.
    # - '[rec][orig]psnr': Calculate PSNR on the scaled videos.
    # - '-f null -': Force null output (don't save a file, just output stats).
    psnr_cmd = [
        'ffmpeg', '-i', recorded_path, '-i', original_path,
        '-lavfi', '[0:v][1:v]scale2ref[rec][orig];[rec][orig]psnr',
        '-f', 'null', '-'
    ]
    try:
        psnr_result = subprocess.run(psnr_cmd,
                                     capture_output=True,
                                     text=True,
                                     timeout=120)
        psnr_match = re.search(r'average:(\d+\.\d+|inf)',
                               psnr_result.stderr)
        if psnr_match:
            val = psnr_match.group(1)
            psnr_val = 100.0 if val == 'inf' else float(val)
            measures.average(video_file, 'video_perf',
                                    'psnr').record(psnr_val)
            logging.info("PSNR: %s", val)
        else:
            logging.warning(
                "Failed to parse PSNR from FFmpeg output. Stderr: %s",
                psnr_result.stderr)
    except Exception as e:
        logging.error("Failed to calculate PSNR: %s", e)

    # SSIM command. Arguments are identical to PSNR above, but calculates
    # Structural Similarity (SSIM) instead.
    ssim_cmd = [
        'ffmpeg', '-i', recorded_path, '-i', original_path,
        '-lavfi', '[0:v][1:v]scale2ref[rec][orig];[rec][orig]ssim',
        '-f', 'null', '-'
    ]
    try:
        ssim_result = subprocess.run(ssim_cmd,
                                     capture_output=True,
                                     text=True,
                                     timeout=120)
        ssim_match = re.search(r'All:(\d+\.\d+)', ssim_result.stderr)
        if ssim_match:
            ssim_val = float(ssim_match.group(1))
            measures.average(video_file, 'video_perf',
                                    'ssim').record(ssim_val)
            logging.info("SSIM: %f", ssim_val)
        else:
            logging.warning(
                "Failed to parse SSIM from FFmpeg output. Stderr: %s",
                ssim_result.stderr)
    except Exception as e:
        logging.error("Failed to calculate SSIM: %s", e)


def finalize_results(chrome_version=None):
    """Dumps metrics and uploads to ResultDB if available."""
    if chrome_version:
        # Tag results with the chrome version for easier tracking in dashboards.
        measures.tag(chrome_version)

    log_dir = os.environ.get('ISOLATED_OUTDIR', '/tmp')
    invocations_dir = os.path.join(log_dir, 'invocations')

    # Dump metrics to the expected location for the result_adapter or
    # other infra tools to find.
    logging.info("Dumping metrics to: %s", invocations_dir)
    measures.dump(invocations_dir)

    # If running in a LUCI environment, try to upload immediately.
    client = result_sink.TryInitClient()
    if client:
        logging.info("LUCI ResultSink detected. Uploading extended properties.")
        try:
            records = {
                measures.TEST_SCRIPT_METRICS_KEY: measures.to_dict()
            }
            client.UpdateInvocationExtendedProperties(records)
            logging.info("Metrics uploaded successfully.")
        except Exception as e:
            logging.error("Failed to upload metrics to ResultSink: %s", e)


def cleanup_binaries(args, chrome_version=None):
    """Cleans up Chrome/Chromedriver directories on remote/local machine."""
    _ = chrome_version

    logging.info("Cleaning up Chrome/Chromedriver directories...")

    try:
        terminate_old_chromedriver(args)
    except Exception as e:
        logging.warning("Error terminating processes in cleanup: %s", e)

    if args.sender in ['localhost', '127.0.0.1', None]:
        # Local cleanup
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
        return

    cleanup_command = {
        'mac': "rm -rf /tmp/chrome* /tmp/chromedriver*",
        'linux': "rm -rf /tmp/chrome* /tmp/chromedriver*",
        'cros': "rm -rf /usr/local/tmp/chrome* /usr/local/tmp/chromedriver*",
        'win': (
            'powershell -Command "Remove-Item -Path '
            f'{WIN_REMOTE_TMP_DIR}/chrome*,'
            f'{WIN_REMOTE_TMP_DIR}/chromedriver* '
            '-Recurse -Force -ErrorAction SilentlyContinue"'
        ),
    }

    send_ssh_command(args.sender, args.username,
                     cleanup_command[args.sender_os],
                     blocking=True)
    logging.info("Cleaned up remote Chrome/Chromedriver directories.")


def start_glances_monitoring(args, csv_remote_path):
    """Starts glances or ChromeOS power monitoring in the background."""
    if args.sender_os == 'cros':
        # ChromeOS: run a background loop writing battery power_now to a file
        # We save the PID to /tmp/cros_power.pid so we can kill it later.
        cros_cmd = (
            "sh -c 'echo $$ > /tmp/cros_power.pid; "
            "while true; do "
            "if [ -f /sys/class/power_supply/battery/power_now ]; then "
            "cat /sys/class/power_supply/battery/power_now; "
            "elif [ -f /sys/class/power_supply/sbat0/power_now ]; then "
            "cat /sys/class/power_supply/sbat0/power_now; "
            "else echo 0; fi >> /tmp/cros_power.txt; "
            "sleep 1; done'"
        )
        logging.info("Starting ChromeOS power monitoring in background...")
        return send_ssh_command(
            args.sender, args.username, cros_cmd, blocking=False)

    # Linux, macOS, Windows: run glances
    python_cmd = 'python' if args.sender_os == 'win' else 'python3'
    glances_cmd = (
        f"{python_cmd} -m glances -t 1 --export csv "
        f"--export-csv-file {csv_remote_path} --quiet"
    )
    logging.info("Starting Glances monitoring on sender...")
    return send_ssh_command(
        args.sender, args.username, glances_cmd, blocking=False)


def stop_glances_monitoring(
    args, glances_proc, csv_remote_path, csv_local_path):
    """Stops the monitoring process, pulls the output file, and cleans up."""
    import shutil
    logging.info("Stopping Glances/Power monitoring...")

    if args.sender_os == 'cros':
        # 1. Kill the ChromeOS background loop using the saved PID
        kill_cmd = (
            "if [ -f /tmp/cros_power.pid ]; then "
            "kill -9 $(cat /tmp/cros_power.pid) 2>/dev/null || true; "
            "rm -f /tmp/cros_power.pid; "
            "fi"
        )
        send_ssh_command(args.sender, args.username, kill_cmd, blocking=True)

        # 2. SCP the power log file back
        remote_log = "/tmp/cros_power.txt"
        if args.sender in ['localhost', '127.0.0.1', None]:
            if os.path.exists(remote_log):
                shutil.copy(remote_log, csv_local_path)
        else:
            key_path = os.path.expanduser('~/.ssh/id_ed25519')
            subprocess.run([
                'scp', '-i', key_path,
                '-o', 'StrictHostKeyChecking=no',
                f'{args.username}@{args.sender}:{remote_log}',
                csv_local_path
            ], check=False, timeout=30)

        # 3. Clean up remote file
        cleanup_cmd = f"rm -f {remote_log}"
        send_ssh_command(
            args.sender, args.username, cleanup_cmd, blocking=True)
        return

    # Non-ChromeOS (Glances):
    # 1. Terminate the background Popen process
    if glances_proc:
        try:
            glances_proc.terminate()
            glances_proc.wait(timeout=5)
        except Exception:
            pass

    # Also send a kill command over SSH just in case
    if args.sender_os == 'win':
        kill_cmd = (
            'powershell -Command "Get-WmiObject Win32_Process | '
            'Where-Object { $_.CommandLine -like \'*glances*\' } | '
            'ForEach-Object { Stop-Process $_.ProcessId -Force }"'
        )
    else:
        kill_cmd = "pkill -f glances"
    send_ssh_command(args.sender, args.username, kill_cmd, blocking=True)

    # 2. SCP the CSV file back to the host
    if args.sender in ['localhost', '127.0.0.1', None]:
        if os.path.exists(csv_remote_path):
            shutil.copy(csv_remote_path, csv_local_path)
    else:
        key_path = os.path.expanduser('~/.ssh/id_ed25519')
        subprocess.run([
            'scp', '-i', key_path,
            '-o', 'StrictHostKeyChecking=no',
            f'{args.username}@{args.sender}:{csv_remote_path}',
            csv_local_path
        ], check=False, timeout=30)

    # 3. Clean up remote file
    cleanup_cmd = f"rm -f {csv_remote_path}"
    send_ssh_command(args.sender, args.username, cleanup_cmd, blocking=True)


def parse_glances_csv_and_record(video_file, csv_local_path, sender_os):
    """Parses the glances/power log and records metrics.

    Args:
        video_file: The name of the video file.
        csv_local_path: The local path to the CSV/log file.
        sender_os: The OS of the sender device.
    """
    import csv
    if not os.path.exists(csv_local_path):
        logging.warning(
            "Monitoring log file not found at %s. Skipping metric parsing.",
            csv_local_path)
        return

    try:
        if sender_os == 'cros':
            # ChromeOS power log is a simple text file with one value per line
            power_draws = []
            with open(csv_local_path, mode='r', encoding='utf-8') as f:
                for line in f:
                    val = line.strip()
                    if val:
                        power_draws.append(float(val))

            if power_draws:
                avg_raw = sum(power_draws) / len(power_draws)
                # Auto-scale: if the raw value is very large, it is in
                # microwatts
                if avg_raw > 10000:
                    avg_power = avg_raw / 1000000.0
                elif avg_raw > 10:
                    avg_power = avg_raw / 1000.0
                else:
                    avg_power = avg_raw

                measures.average(
                    video_file, 'video_perf',
                    'power_consumption_watts').record(avg_power)
                logging.info("ChromeOS Average Power Draw: %.2f W", avg_power)
            return

        # Non-ChromeOS (Glances CSV)
        cpu_usages = []
        power_draws = []

        import re
        def parse_float(val):
            """Parses a float value from a string, ignoring formatting."""
            if not val:
                return None
            match = re.search(r'[-+]?\d+(?:\.\d+)?', val)
            return float(match.group(0)) if match else None

        with open(csv_local_path, mode='r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            for row in reader:
                # Glances CPU key
                cpu_val = parse_float(row.get('cpu.total'))
                if cpu_val is not None:
                    cpu_usages.append(cpu_val)

                # Check for battery/power keys
                power_val = parse_float(row.get('sensors.Battery.value'))
                if power_val is not None:
                    power_draws.append(power_val)

        if cpu_usages:
            avg_cpu = sum(cpu_usages) / len(cpu_usages)
            measures.average(
                video_file, 'video_perf', 'cpu_utilization').record(avg_cpu)
            logging.info("Average CPU utilization: %.2f%%", avg_cpu)

        if power_draws:
            avg_power = sum(power_draws) / len(power_draws)
            measures.average(
                video_file, 'video_perf',
                'power_consumption_watts').record(avg_power)
            logging.info("Average Power Draw: %.2f W", avg_power)

    except Exception as e:
        logging.error("Failed to parse monitoring log: %s", e)

