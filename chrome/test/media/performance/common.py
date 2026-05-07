# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A common module for media performance tests."""

import logging
import multiprocessing
import os
import subprocess
import sys
import time
import json
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
        'name': '1080p60fpsHEVC_boat_sync.mp4',
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
}

SENDER_TERMINATE_DRIVER_CMD = {
    'mac': (
        'killall chromedriver && killall "Google Chrome for Testing"'
    ),
    'win': (
        'powershell -Command "Stop-Process -Name chromedriver,chrome '
        '-ErrorAction SilentlyContinue; '
        'taskkill /F /IM chromedriver.exe /IM chrome.exe /T '
        '/FI \'STATUS eq RUNNING\' /FI \'SESSION ne 0\'"'
    ),
    'linux': (
        'pkill -f chromedriver || true && pkill -f chrome || true'
    ),
}


WIN_REMOTE_TMP_DIR = 'C:/cft_temp'


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
                     SENDER_TERMINATE_DRIVER_CMD[args.sender_os])

    for _ in range(5):
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
        if not version or v['version'] == version:
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

        subprocess.run(
            f"curl -L {chrome_url} -o {tmp_dir}/{chrome_zip} && "
            f"curl -L {driver_url} -o {tmp_dir}/{driver_zip} && "
            f"unzip -o {tmp_dir}/{chrome_zip} -d {tmp_dir} && "
            f"unzip -o {tmp_dir}/{driver_zip} -d {tmp_dir}",
            shell=True, check=True, timeout=120)

        chrome_dir = chrome_zip.replace('.zip', '')
        driver_dir = driver_zip.replace('.zip', '')
        remote_app_path = (f"{tmp_dir}/{chrome_dir}/"
                           "Google Chrome for Testing.app")
        if sys.platform == 'linux':
             remote_app_path = f"{tmp_dir}/{chrome_dir}/chrome"

        remote_chromedriver_path = f"{tmp_dir}/{driver_dir}/chromedriver"

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

        send_ssh_command(
            args.sender, args.username,
            (f"curl -L {chrome_url} -o {remote_tmp_dir}/{chrome_zip} && "
             f"curl -L {driver_url} -o {remote_tmp_dir}/{driver_zip} && "
             f"unzip -o {remote_tmp_dir}/{chrome_zip} -d {remote_tmp_dir} && "
             f"unzip -o {remote_tmp_dir}/{driver_zip} -d {remote_tmp_dir}"),
            blocking=True)

        remote_app_path = (
            f"{remote_tmp_dir}/{chrome_zip.replace('.zip', '')}/Google "
            "Chrome for Testing.app")
        remote_chromedriver_path = (
            f"{remote_tmp_dir}/{driver_zip.replace('.zip', '')}/chromedriver")

        send_ssh_command(
            args.sender, args.username,
            (f"xattr -cr {remote_tmp_dir}/{chrome_zip.replace('.zip', '')} && "
             f"xattr -cr {remote_tmp_dir}/{driver_zip.replace('.zip', '')}"),
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

        send_ssh_command(
            args.sender, args.username,
            (f"curl -L {chrome_url} -o {remote_tmp_dir}/{chrome_zip} && "
             f"curl -L {driver_url} -o {remote_tmp_dir}/{driver_zip} && "
             f"unzip -o {remote_tmp_dir}/{chrome_zip} -d {remote_tmp_dir} && "
             f"unzip -o {remote_tmp_dir}/{driver_zip} -d {remote_tmp_dir}"),
            blocking=True)

        chrome_dir = chrome_zip.replace('.zip', '')
        driver_dir = driver_zip.replace('.zip', '')
        remote_app_path = f"{remote_tmp_dir}/{chrome_dir}/chrome"
        remote_chromedriver_path = f"{remote_tmp_dir}/{driver_dir}/chromedriver"

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

        # Download and Unzip using a single robust PowerShell command
        logging.info("Downloading and unzipping Chrome and Chromedriver...")
        setup_cmd = (
            f"powershell -Command \"$ErrorActionPreference = 'Stop'; "
            f"curl.exe -L '{chrome_url}' -o '{chrome_zip_path}'; "
            f"curl.exe -L '{driver_url}' -o '{driver_zip_path}'; "
            f"Expand-Archive -Path '{chrome_zip_path}' "
            f"-DestinationPath '{remote_tmp_dir}' -Force; "
            f"Expand-Archive -Path '{driver_zip_path}' "
            f"-DestinationPath '{remote_tmp_dir}' -Force\""
        )
        result = send_ssh_command(args.sender, args.username, setup_cmd,
                                  blocking=True)
        if result.returncode != 0:
            raise RuntimeError(f"Failed to setup Chrome/Chromedriver on "
                               f"Windows: {result.stderr}")

        chrome_dir = chrome_zip_name.replace('.zip', '')
        driver_dir = driver_zip_name.replace('.zip', '')
        remote_app_path = f'{remote_tmp_dir}/{chrome_dir}/chrome.exe'

        # Create and run the batch script
        batch_script_content = (
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
             f'\'{batch_script_path}\' /sc ONCE /st 23:59 /f"'),
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


def dump_remote_logs(args):
    """Tries to dump the remote Chromedriver console logs to the local log."""
    logging.error("Dumping remote console logs:")
    if args.sender_os == 'win':
        # On Windows, the log is in the temp dir under a dynamic driver dir.
        log_cmd = (f'powershell -Command "Get-Content {WIN_REMOTE_TMP_DIR}/'
                   '*/chromedriver_console.log"')
    else:
        log_cmd = 'cat /tmp/chromedriver_console.log'

    log_result = send_ssh_command(args.sender,
                                  args.username,
                                  log_cmd,
                                  blocking=True)
    if log_result.stdout.strip() or log_result.stderr.strip():
        logging.error("REMOTE CONSOLE LOG:\nSTDOUT: %s\nSTDERR: %s",
                      log_result.stdout, log_result.stderr)


def wait_for_chromedriver(args):
    """Waits for the new chromedriver to be ready by checking its status URL."""
    logging.info("Starting Chromedriver status check...")
    for i in range(10):
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
    dump_remote_logs(args)
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

    if tunnel_proc and tunnel_proc.poll() is None:
        tunnel_proc.terminate()
        logging.info("Terminated tunnel.")

    cleanup_command = {
        'mac': (
            "rm -rf /tmp/chrome-mac-* /tmp/chromedriver-mac-* "
            "/tmp/*.zip"
        ),
        'win': (
            f'powershell -Command "Remove-Item -Path {WIN_REMOTE_TMP_DIR} '
            '-Recurse -Force -ErrorAction SilentlyContinue"'
        ),
        'linux': (
            "rm -rf /tmp/chrome-linux64-* /tmp/chromedriver-linux64-* "
            "/tmp/*.zip"
        ),
    }

    send_ssh_command(args.sender, args.username,
                     cleanup_command[args.sender_os])
    logging.info("Cleaned up tmp files on remote machine.")


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
